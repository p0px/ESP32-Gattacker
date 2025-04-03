#include <stdint.h>
#include "string.h"
#include "nvs_flash.h"
#include "nvs.h"

#include "settings.h"

#define TAG "settings"

const char* version = "v0.3.4";

const char* BASE_PATH = "/littlefs";

// Define the MAC address in hexadecimal
uint8_t bt_mac_address[] = {0x13, 0x37, 0x22, 0x33, 0x44, 0x55};
uint8_t ap_mac_address[] = {0x00, 0x00, 0x12, 0x34, 0x56, 0x78};
uint8_t st_mac_address[] = {0x13, 0x33, 0x77, 0x13, 0x37, 0x67};

const uint8_t default_bt_mac_address[] = {0x13, 0x37, 0x22, 0x33, 0x44, 0x55};
const uint8_t default_ap_mac_address[] = {0x00, 0x00, 0x12, 0x34, 0x56, 0x78};
const uint8_t default_st_mac_address[] = {0x13, 0x33, 0x77, 0x13, 0x37, 0x67};

// AP IP/DHCP settings

char WIFI_SSID[32];
char WIFI_PASS[32];

// NVS functions
esp_err_t init_nvs() {
  // Initialize NVS
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    // NVS partition was truncated and needs to be erased
    // Retry nvs_flash_init
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  return ret;
};

esp_err_t write_string_to_nvs(const char* key, const char* value) {
  nvs_handle_t my_handle;
  esp_err_t err;

  // Open
  err = nvs_open("loot", NVS_READWRITE, &my_handle);
  if (err != ESP_OK) return err;

  // Write
  err = nvs_set_str(my_handle, key, value);
  if (err != ESP_OK) return err;

  // Commit written value.
  err = nvs_commit(my_handle);
  if (err != ESP_OK) return err;

  // Close
  nvs_close(my_handle);
  return ESP_OK;
};

esp_err_t write_uint32_to_nvs(const char* key, uint32_t value) {
  nvs_handle_t my_handle;
  esp_err_t err;

  // Open the NVS storage with a given namespace ("loot" in this case)
  err = nvs_open("loot", NVS_READWRITE, &my_handle);
  if (err != ESP_OK) return err;

  // Write the uint32_t value to NVS using the provided key
  err = nvs_set_u32(my_handle, key, value);
  if (err != ESP_OK) return err;

  // Commit the written value to make sure it's saved
  err = nvs_commit(my_handle);
  if (err != ESP_OK) return err;

  // Close the NVS handle
  nvs_close(my_handle);

  return ESP_OK; // Return success if all operations succeeded
};

esp_err_t read_uint32_from_nvs(const char* key, uint32_t* value) {
  nvs_handle_t my_handle;
  esp_err_t err;

  // Open the NVS storage with the same namespace ("loot" in this case)
  err = nvs_open("loot", NVS_READONLY, &my_handle);
  if (err != ESP_OK) return err;

  // Read the uint32_t value from NVS using the provided key
  err = nvs_get_u32(my_handle, key, value);
  if (err != ESP_OK) {
    nvs_close(my_handle);
    return err;
  }

  // Close the NVS handle
  nvs_close(my_handle);

  return ESP_OK; // Return success if the read operation succeeded
};

esp_err_t read_string_from_nvs(const char* key, char* buffer, size_t buffer_size) {
  nvs_handle_t my_handle;
  esp_err_t err = nvs_open("loot", NVS_READONLY, &my_handle);
  if (err != ESP_OK) return err;

  size_t required_size = 0;
  err = nvs_get_str(my_handle, key, NULL, &required_size);
  if (err != ESP_OK) {
    nvs_close(my_handle);
    return err;
  }

  if (required_size > buffer_size) {
    nvs_close(my_handle);
    return ESP_ERR_NVS_INVALID_LENGTH;
  }

  err = nvs_get_str(my_handle, key, buffer, &required_size);
  nvs_close(my_handle);
  return err;
};

esp_err_t write_blob_to_nvs(const char* key, const void* data, size_t length) {
  nvs_handle_t my_handle;
  esp_err_t err;

  // Open
  err = nvs_open("loot", NVS_READWRITE, &my_handle);
  if (err != ESP_OK) return err;

  // Write
  err = nvs_set_blob(my_handle, key, data, length);
  if (err != ESP_OK) return err;

  // Commit written value.
  err = nvs_commit(my_handle);
  if (err != ESP_OK) return err;

  // Close
  nvs_close(my_handle);
  return ESP_OK;
};

esp_err_t read_blob_from_nvs(const char* key, void* data, size_t length) {
  nvs_handle_t my_handle;
  esp_err_t err = nvs_open("loot", NVS_READONLY, &my_handle);
  if (err != ESP_OK) return err;

  // Determine the size of the required buffer
  size_t required_size = 0;
  err = nvs_get_blob(my_handle, key, NULL, &required_size);
  if (err != ESP_OK) {
    nvs_close(my_handle);
    return err;
  }

  // Check if provided buffer is large enough
  if (required_size > length) {
    nvs_close(my_handle);
    length = required_size;
    return ESP_ERR_NVS_INVALID_LENGTH;
  }

  // Read the blob
  err = nvs_get_blob(my_handle, key, data, &required_size);
  nvs_close(my_handle);

  // Set the actual length of the data read
  length = required_size;
  return err;
};

void read_settings() {
  init_nvs();

  esp_err_t ret = read_string_from_nvs("wifi_ssid", WIFI_SSID, sizeof(WIFI_SSID));
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Settings: no ssid");
    strncpy(WIFI_SSID, DEFAULT_WIFI_SSID, sizeof(WIFI_SSID) - 1);
    WIFI_SSID[sizeof(WIFI_SSID) - 1] = '\0';
    write_string_to_nvs("wifi_ssid", WIFI_SSID);
  }

  ESP_LOGI(TAG, "wifi_ssid: %s", WIFI_SSID);

  ret = read_string_from_nvs("wifi_pass", WIFI_PASS, sizeof(WIFI_PASS));
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Settings: no wifi_pass");
    strncpy(WIFI_PASS, DEFAULT_WIFI_PASS, sizeof(WIFI_PASS) - 1);
    WIFI_PASS[sizeof(WIFI_PASS) - 1] = '\0';
    write_string_to_nvs("wifi_pass", WIFI_PASS);
  }

  ESP_LOGI(TAG, "wifi_pass: %s", WIFI_PASS);

  ret = read_blob_from_nvs("bt_mac", bt_mac_address, 6);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Settings: no bt_mac ");
    memcpy(bt_mac_address, default_bt_mac_address, 6);
  }

  ret = read_blob_from_nvs("ap_mac", ap_mac_address, 6);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Settings: no ap_mac ");
    memcpy(ap_mac_address, default_ap_mac_address, 6);
  }

  ret = read_blob_from_nvs("st_mac", st_mac_address, 6);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Settings: no st_mac ");
    memcpy(st_mac_address, default_st_mac_address, 6);
  }

  ESP_ERROR_CHECK(esp_iface_mac_addr_set(bt_mac_address, ESP_MAC_BT));
  ESP_ERROR_CHECK(esp_iface_mac_addr_set(ap_mac_address, ESP_MAC_WIFI_SOFTAP));
  ESP_ERROR_CHECK(esp_iface_mac_addr_set(st_mac_address, ESP_MAC_WIFI_STA));
};
