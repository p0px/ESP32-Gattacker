#ifndef GATTATTACK_H
#define GATTATTACK_H

#ifdef __cplusplus

extern "C" {
#endif

#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include "esp_mac.h"
#include "esp_gap_ble_api.h"
#include "esp_gattc_api.h"
#include "esp_gatt_defs.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"
#include "esp_bt_defs.h"
#include "esp_bt.h"
#include "esp_gatts_api.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include <inttypes.h>

/* Structs */
struct desc_t {
  esp_bt_uuid_t desc_uuid;
  uint16_t desc_handle;
  esp_attr_value_t value;
  esp_gatt_perm_t perm;
};

struct char_t {
  esp_bt_uuid_t char_uuid;
  uint16_t char_handle;
  esp_gatt_char_prop_t property;
  desc_t *descs;
  uint8_t num_descs;
  esp_attr_value_t value;
};

struct service_t {
  esp_bt_uuid_t service_uuid;
  uint16_t service_handle;
  uint16_t start_handle;
  uint16_t end_handle;
  esp_gatt_srvc_id_t service_id;
  char_t *chars;
  uint8_t num_chars;
  uint8_t num_handles;
};

struct clone_t {
  uint8_t *target;
  uint16_t gatts_if;
  uint16_t gattc_if;

  uint16_t target_client_conn_id;
  uint16_t victim_client_conn_id;

  uint16_t server_conn_id;
  uint16_t server_trans_id;
  bool server_last_write_needs_rsp;

  uint16_t target_gattc_mtu;

  service_t *services;
  uint8_t num_services;
  esp_bd_addr_t target_bda;
} clone;

typedef struct {
  uint8_t *prepare_buf;
  int prepare_len;
} prepare_type_env_t;

esp_ble_scan_params_t ble_scan_params = {
  .scan_type = BLE_SCAN_TYPE_ACTIVE,
  .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
  .scan_filter_policy = BLE_SCAN_FILTER_ALLOW_ALL,
  .scan_interval = 0x50,
  .scan_window = 0x30,
  .scan_duplicate = BLE_SCAN_DUPLICATE_DISABLE
};

esp_ble_adv_params_t adv_params = {
  .adv_int_min        = 0x20,
  .adv_int_max        = 0x20,
  .adv_type           = ADV_TYPE_IND,
  .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
  //.peer_addr            =
  //.peer_addr_type       =
  .channel_map        = ADV_CHNL_ALL,
  .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

/* Function Declarations */
char* bytes_to_hex_string(const uint8_t *data, size_t data_len);
void print_uuid(esp_bt_uuid_t uuid, char* out);
void start_gatt_attack(uint8_t *target);
void stop_gatt_attack();

#ifdef __cplusplus
}
#endif

#endif // GATTATTACK_H
