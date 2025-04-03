#include "ble/ble.h"

#define TAG "ble"

bool initialized = false;

#define MAX_DEVICES 50

int device_count = 0;
SemaphoreHandle_t scan_complete_semaphore;

// Function to add a device to the array
static void add_device(esp_bd_addr_t bd_addr, const char *name, int rssi) {
  // Check if the device is already in the list
  for (int i = 0; i < device_count; i++) {
    if (memcmp(devices[i].bd_addr, bd_addr, sizeof(esp_bd_addr_t)) == 0) {
      return;
    }
  }

  // Add the new device if the array is not full
  if (device_count < MAX_DEVICES) {
    memcpy(devices[device_count].bd_addr, bd_addr, sizeof(esp_bd_addr_t));
    strncpy(devices[device_count].name, name, ESP_BLE_ADV_DATA_LEN_MAX);
    devices[device_count].name[ESP_BLE_ADV_DATA_LEN_MAX] = '\0';
    devices[device_count].rssi = rssi;
    device_count++;
  }
}

// https://github.com/espressif/arduino-esp32/blob/master/libraries/BLE/src/BLEDevice.cpp#L353
static void bluedroid_init() {
  esp_err_t errRc = ESP_OK;
  esp_bluedroid_status_t bt_state = esp_bluedroid_get_status();
  if (bt_state == ESP_BLUEDROID_STATUS_UNINITIALIZED) {
    errRc = esp_bluedroid_init();
    if (errRc != ESP_OK) {
      ESP_LOGE(TAG, "esp_bluedroid_init: rc=%d %s", errRc, esp_err_to_name(errRc));
      return;
    }
  }

  if (bt_state != ESP_BLUEDROID_STATUS_ENABLED) {
    errRc = esp_bluedroid_enable();
    if (errRc != ESP_OK) {
      ESP_LOGE(TAG, "esp_bluedroid_enable: rc=%d %s", errRc, esp_err_to_name(errRc));
      return;
    }
  }

  errRc = esp_ble_gap_set_device_name("");
  if (errRc != ESP_OK) {
    ESP_LOGE(TAG, "esp_ble_gap_set_device_name: rc=%d %s", errRc, esp_err_to_name(errRc));
    return;
  };

  vTaskDelay(200 / portTICK_PERIOD_MS);  // Delay for 200 msecs as a workaround to an apparent Arduino environment issue.
  initialized = true;
}

// https://github.com/espressif/arduino-esp32/blob/1cdd8e8ce0d0003518fb2b68365bb4f2809f11d1/cores/esp32/esp32-hal-bt.c#L49
bool ble_stack_init() {
  if (initialized) {
    return true;
  }

  esp_bt_mode_t esp_bt_mode = ESP_BT_MODE_BLE;
  esp_bt_controller_config_t cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
  
  if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_ENABLED) {
    return true;
  }
  esp_err_t ret;
  if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_IDLE) {
    if ((ret = esp_bt_controller_init(&cfg)) != ESP_OK) {
      ESP_LOGE(TAG, "initialize controller failed: %s", esp_err_to_name(ret));
      return false;
    }
    while (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_IDLE) {}
  }
  if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_INITED) {
    if ((ret = esp_bt_controller_enable(esp_bt_mode)) != ESP_OK) {
      ESP_LOGE(TAG, "BT Enable mode=%d failed %s", esp_bt_mode, esp_err_to_name(ret));
      return false;
    }
  }
  if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_ENABLED) {
    bluedroid_init();
    return true;
  }
  ESP_LOGE(TAG, "BT Start failed");
  return false;
}

// https://github.com/espressif/arduino-esp32/blob/1cdd8e8ce0d0003518fb2b68365bb4f2809f11d1/cores/esp32/esp32-hal-bt.c#L93
void ble_stack_deinit(bool release_memory) {
  if (!initialized) {
    return;
  }

  esp_bluedroid_disable();
  esp_bluedroid_deinit();
  esp_bt_controller_disable();
  esp_bt_controller_deinit();

  if (release_memory) {
    esp_bt_controller_mem_release(ESP_BT_MODE_BTDM);
  } else {
    initialized = false;
  }
}

// GAP callback used for populating devices from BLE advertisements
static void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
  switch (event) {
    case ESP_GAP_BLE_SCAN_RESULT_EVT: {
      switch (param->scan_rst.search_evt) {
        case ESP_GAP_SEARCH_INQ_RES_EVT: {
          uint8_t adv_name_len = 0;
          uint8_t *adv_name = esp_ble_resolve_adv_data(param->scan_rst.ble_adv,
                                                      ESP_BLE_AD_TYPE_NAME_CMPL,
                                                      &adv_name_len);
          char name[ESP_BLE_ADV_DATA_LEN_MAX + 1] = "";
          if (adv_name && adv_name_len > 0) {
            strncpy(name, (char *)adv_name, adv_name_len);
            name[adv_name_len] = '\0';
          }

          if (param->scan_rst.flag & 0x02) {
            // Only target connectable devices for now
            add_device(param->scan_rst.bda, name, param->scan_rst.rssi);
          }
          break;
        }
        case ESP_GAP_SEARCH_INQ_CMPL_EVT: {
          xSemaphoreGive(scan_complete_semaphore);
          break;
        }
        default:
          break;
      }
      break;
    }

    default:
      break;
  }
}

// Function to initiate BLE scanning for a specified duration (in seconds) with a void callback
void start_ble_scan(int scan_duration_seconds, void (*callback)()) {
  scan_complete_semaphore = xSemaphoreCreateBinary();
  device_count = 0;

  esp_ble_gap_register_callback(esp_gap_cb);
  esp_ble_gap_set_scan_params(&default_scan_params);

  ESP_LOGI(TAG, "Starting BLE scan for %d seconds", scan_duration_seconds);
  esp_ble_gap_start_scanning(scan_duration_seconds);

  // Wait for the semaphore to be given when scanning is complete
  if (xSemaphoreTake(scan_complete_semaphore, (scan_duration_seconds) * 1000 / portTICK_PERIOD_MS) + 1 == pdTRUE) {
    if (callback != NULL) {
      callback();
    } else {
      for (int i = 0; i < device_count; i++) {
        ESP_LOGI(TAG, "Device %d - BDADDR: %02x:%02x:%02x:%02x:%02x:%02x, Name: %s, RSSI: %d",
                 i,
                 devices[i].bd_addr[0], devices[i].bd_addr[1],
                 devices[i].bd_addr[2], devices[i].bd_addr[3],
                 devices[i].bd_addr[4], devices[i].bd_addr[5],
                 devices[i].name, devices[i].rssi);
      }
    }
  }

  vSemaphoreDelete(scan_complete_semaphore);
}
