#ifndef BLESTACK_H
#define BLESTACK_H

#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_mac.h"
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gattc_api.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"
#include "esp_gap_ble_api.h"
#include "esp_log.h"

#include "esp_log.h"
#include "settings.h"

// Struct to hold scanned device information
typedef struct {
  esp_bd_addr_t bd_addr;
  char name[ESP_BLE_ADV_DATA_LEN_MAX + 1];
  int rssi;
} ble_device_t;

ble_device_t devices[50];

int device_count;
bool ble_stack_init();
void ble_stack_deinit(bool release_memory);
void start_ble_scan(int scan_duration_seconds, void (*callback)());

#ifdef __cplusplus
}
#endif

#endif // BLESTACK_H
