#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"

#include "ble/GattAttackApp.h"

#define TAG "GattAttackApp"

bool hooks_enabled = false;
bool hook_block = false;
char *hook_ret = NULL;

extern LEDStrip* leds;
TaskHandle_t gattTaskHandle = NULL;
GattAttackState gattAttackState = GATT_ATTACK_STATE_HOME;

bool should_run_hooks() {
  if (num_clients == 0 || !hooks_enabled) {
    return false;
  }

  return true;
};

char* run_hook(const char *type, const char *uuid, const char *value) {
  cJSON *res = cJSON_CreateObject();

  if (strcmp(type, "write") == 0) {
    cJSON_AddBoolToObject(res, "onWrite", true);
  } else {
    cJSON_AddBoolToObject(res, "onRead", true);
  }
  
  cJSON_AddStringToObject(res, "uuid", uuid);
  cJSON_AddStringToObject(res, "value", value);
  char *json = cJSON_Print(res);

  // send to ws and wait
  msg_clients(json);
  free(json);
  cJSON_Delete(res);

  int timeout = 0;
  hook_block = true;
  while (hook_block) {
    // return original value if timeout of 800ms
    if (timeout >= 800) {
      hook_ret = strdup(value);
      hook_block = false;
    }
    vTaskDelay(10 / portTICK_PERIOD_MS);
    timeout += 10;
  }

  return hook_ret;
};

void send_output(std::string output, bool isJSON) {
  if (isJSON) { 
    msg_clients(output);
    return;
  }

  cJSON *res = cJSON_CreateObject();
  cJSON_AddStringToObject(res, "msg", output.c_str());
  char *json = cJSON_Print(res);

  ESP_LOGI(TAG, "%s", output.c_str());

  msg_clients(json);
  free(json);
  cJSON_Delete(res);
};

void GattAttackApp::scan(void* params) {
  GattAttackApp::scan();
  vTaskDelete(NULL);
};

void GattAttackApp::sendState(int state) {
  cJSON *res = cJSON_CreateObject();
  cJSON_AddNumberToObject(res, "state", state);
  cJSON_AddBoolToObject(res, "hooks_enabled", hooks_enabled);
  char *json = cJSON_Print(res);

  msg_clients(json);
  
  free(json);
  cJSON_Delete(res);
};

void GattAttackApp::scan() {
  gattAttackState = GATT_ATTACK_STATE_SCANNING;
  GattAttackApp::sendState((int)gattAttackState);
  
  // scan for devices
  ble_stack_init();

  start_ble_scan(10, NULL);
  
  ble_stack_deinit(false);

  ESP_LOGI(TAG, "Scan Finished. Got %i Devices", device_count);

  cJSON *res = cJSON_CreateObject();
  cJSON_AddBoolToObject(res, "scan_done", true);
  char *json = cJSON_Print(res);
  
  msg_clients(json);
  
  free(json);
  cJSON_Delete(res);

  gattAttackState = GATT_ATTACK_STATE_SCAN_RES;
  GattAttackApp::sendState((int)gattAttackState);
};

void GattAttackApp::stop_attack() {
  stop_gatt_attack();

  if (gattTaskHandle != NULL) { 
    vTaskDelete(gattTaskHandle);
    gattTaskHandle = NULL;
  }

  gattAttackState = GATT_ATTACK_STATE_HOME;
  GattAttackApp::sendState((int)gattAttackState);
};

void GattAttackApp::start_attack(esp_bd_addr_t target) {
  GattAttackParams* params = new GattAttackParams{target};
  esp_err_t ret = xTaskCreate(&GattAttackApp::atk_task, "gatk", 20000, params, 0, &gattTaskHandle);
  if (ret == ESP_FAIL) {
    ESP_LOGE(TAG, "gatk task err: %s", esp_err_to_name(ret));
  }
  gattAttackState = GATT_ATTACK_STATE_START;
  GattAttackApp::sendState((int)gattAttackState);
};

void GattAttackApp::atk_task(void *taskParams) {
  GattAttackParams *params = static_cast<GattAttackParams*>(taskParams);
  start_gatt_attack(params->target);
  while(1) {
    vTaskDelay(20 / portTICK_PERIOD_MS);
  }
};

void GattAttackApp::webEvent(GattAttackWebEventParams *ps) {
  if (strcmp(ps->action, "status") == 0) {
    return GattAttackApp::sendState((int)gattAttackState);
  }

  if (strcmp(ps->action, "start") == 0) {
    leds->stop_animations();

    // Validate ID sent
    if (ps->id < 0 || ps->id > device_count) {
      cJSON *root = cJSON_CreateObject();
      cJSON_AddBoolToObject(root, "success", false);
      cJSON_AddStringToObject(root, "error", "Invalid Device ID");

      // Convert the root object to a formatted JSON string
      char *json_string = cJSON_Print(root);
      
      // Send json and free
      msg_clients(json_string);
      free(json_string);
      cJSON_Delete(root);
      return;
    }

    return GattAttackApp::start_attack(devices[ps->id].bd_addr);
  }

  if (strcmp(ps->action, "stop") == 0) {
    GattAttackApp::stop_attack();
    return leds->run_animations();
  }

  if (strcmp(ps->action, "scan") == 0) {
    // Start scan task
    xTaskCreate(&GattAttackApp::scan, "gatk", 4000, NULL, 0, NULL);
    return;
  }

  if (strcmp(ps->action, "results") == 0) {
    // Create the root JSON object
    cJSON *root = cJSON_CreateObject();
    cJSON *results_array = cJSON_CreateArray();
    cJSON_AddItemToObject(root, "results", results_array);

    for (int i = 0; i < device_count; i++) {
      // Create a JSON object for each device
      cJSON *device_obj = cJSON_CreateObject();
      char mac_str[18];
      snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
              devices[i].bd_addr[0], devices[i].bd_addr[1],
              devices[i].bd_addr[2], devices[i].bd_addr[3],
              devices[i].bd_addr[4], devices[i].bd_addr[5]);

      // Add properties to the device object
      cJSON_AddNumberToObject(device_obj, "id", i);
      cJSON_AddStringToObject(device_obj, "mac", mac_str);
      cJSON_AddStringToObject(device_obj, "name", devices[i].name);
      cJSON_AddNumberToObject(device_obj, "rssi", devices[i].rssi);

      // Add the device object to the array
      cJSON_AddItemToArray(results_array, device_obj);
    }

    // Convert the root object to a formatted JSON string
    char *json_string = cJSON_Print(root);
    
    // Send json and free
    msg_clients(json_string);
    free(json_string);
    cJSON_Delete(root);
    return;
  }
  
  if (strcmp(ps->action, "hookret") == 0) {
    hook_block = false;
    hook_ret = strdup(ps->hook_ret);
    return;
  }

  if (strcmp(ps->action, "enable_hooks") == 0) {
    hooks_enabled = ps->enable;
    char out[32];
    snprintf(out, sizeof(out), "hooks %s", hooks_enabled ? "enabled" : "disabled");
    send_output(out);
    sendState(gattAttackState);
    return;
  }
};