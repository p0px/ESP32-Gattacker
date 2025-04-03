#include "wifi/ws.h"

#define TAG "WS"

enum OPTS {
  WS_OPT_SET_WIFI,
  WS_OPT_REBOOT,
  WS_OPT_GATTATTACK,
};

struct async_resp_arg {
  httpd_handle_t hd;
  int fd;
  char *res;
};

struct ws_client_t {
  httpd_handle_t hd;
  int fd;
};

int num_clients = 0;
ws_client_t *clients;

static void add_client(const ws_client_t *new_client) {
  ws_client_t *new_clients = (ws_client_t *)realloc(clients, (num_clients + 1) * sizeof(ws_client_t));
  if (new_clients == NULL) {
    ESP_LOGE(TAG, "Adding client no mem");
    return;
  }

  clients = new_clients;
  clients[num_clients] = *new_client;
  num_clients++;
}

static void remove_client_by_fd(int fd) {
  int found_index = -1;
  for (int i = 0; i < num_clients; i++) {
    if (clients[i].fd == fd) {
      found_index = i;
      break;
    }
  }

  if (found_index == -1) {
    return;
  }

  for (int i = found_index; i < num_clients - 1; i++) {
    clients[i] = clients[i + 1];
  }

  num_clients--;

  if (num_clients > 0) {
    ws_client_t *new_clients = (ws_client_t *)realloc(clients, num_clients * sizeof(ws_client_t));
    if (new_clients != NULL) {
      clients = new_clients;
    }
  } else {
    free(clients);
    clients = NULL;
  }
}

void ws_on_close_handler(httpd_handle_t hd, int sockfd) {
  remove_client_by_fd(sockfd);
}

static void ws_async_send(void *arg) {
  struct async_resp_arg *resp_arg = (struct async_resp_arg *)arg;
  httpd_handle_t hd = resp_arg->hd;
  int fd = resp_arg->fd;
  httpd_ws_frame_t ws_pkt;

  memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));

  if (resp_arg->res != NULL) {
    ws_pkt.payload = (uint8_t *)resp_arg->res;
    ws_pkt.len = strlen(resp_arg->res);
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
    
    httpd_ws_send_frame_async(hd, fd, &ws_pkt);
    free(resp_arg->res);
  }

  free(resp_arg);
}

void msg_clients(std::string msg) {
  for (int i = 0; i < num_clients; i++) {
    ws_client_t cli = clients[i];
    struct async_resp_arg *resp_arg = (struct async_resp_arg *)malloc(sizeof(struct async_resp_arg));
    if (resp_arg == NULL) {
      ESP_LOGE(TAG, "Failed to allocate memory for async_resp_arg");
      continue;
    }

    resp_arg->hd = cli.hd;
    resp_arg->fd = cli.fd;
    resp_arg->res = (char *)malloc(msg.size() + 1);
    if (resp_arg->res == NULL) {
      ESP_LOGE(TAG, "Failed to allocate memory for response");
      free(resp_arg);
      continue;
    }
    strcpy(resp_arg->res, msg.c_str());

    esp_err_t ret = httpd_queue_work(cli.hd, ws_async_send, resp_arg);
    if (ret != ESP_OK) {
      ESP_LOGI(TAG, "Failed to queue work for async response");
      free(resp_arg->res);
      free(resp_arg);
    }
  }
}

esp_err_t ws_handler(httpd_req_t *req) {
  if (req->method == HTTP_GET) {
    ESP_LOGI(TAG, "New Connection");

    ws_client_t cli = {
      .hd = req->handle,
      .fd = httpd_req_to_sockfd(req)
    };
    add_client(&cli);

    return ESP_OK;
  }

  httpd_ws_frame_t ws_pkt;
  uint8_t *buf = NULL;
  memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
  ws_pkt.type = HTTPD_WS_TYPE_TEXT;

  esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "httpd_ws_recv_frame failed to get frame length with %d", ret);
    return ret;
  }
  
  if (ws_pkt.len) {
    buf = (uint8_t *)calloc(1, ws_pkt.len + 1);
    if (buf == NULL) {
      ESP_LOGE(TAG, "Failed to allocate memory for buf");
      return ESP_ERR_NO_MEM;
    }
    ws_pkt.payload = buf;
    ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
    if (ret != ESP_OK) {
      ESP_LOGE(TAG, "httpd_ws_recv_frame failed with %d", ret);
      free(buf);
      return ret;
    }
    //ESP_LOGI(TAG, "Got packet with message: %s", ws_pkt.payload);
  }

  if (ws_pkt.type == HTTPD_WS_TYPE_TEXT) {
    struct async_resp_arg *resp_arg = (struct async_resp_arg *)malloc(sizeof(struct async_resp_arg));
    if (resp_arg == NULL) {
      ESP_LOGE(TAG, "Failed to allocate memory for async_resp_arg");
      free(buf);
      return ESP_ERR_NO_MEM;
    }
    resp_arg->hd = req->handle;
    resp_arg->fd = httpd_req_to_sockfd(req);

    cJSON *res = cJSON_CreateObject();
    cJSON *root = cJSON_Parse((char *)ws_pkt.payload);
    if (root == NULL) {
      ESP_LOGE(TAG, "Failed to parse JSON");
      cJSON_AddBoolToObject(res, "success", false);
      cJSON_AddStringToObject(res, "error", "Invalid JSON format");
      resp_arg->res = cJSON_Print(res);
      free(buf);
      if (httpd_queue_work(req->handle, ws_async_send, resp_arg) != ESP_OK) {
        free(resp_arg->res);
        free(resp_arg);
      }
      cJSON_Delete(res);
      return ESP_OK;
    }

    cJSON *option = cJSON_GetObjectItem(root, "opt");
    if (cJSON_IsNumber(option)) {
      OPTS opt = (OPTS)option->valueint;
      switch (opt) {
        case WS_OPT_SET_WIFI: {
          cJSON *ssid = cJSON_GetObjectItem(root, "ssid");
          cJSON *pass = cJSON_GetObjectItem(root, "pass");
          if ((cJSON_IsString(ssid) && ssid->valuestring != NULL) && 
              (cJSON_IsString(pass) && pass->valuestring != NULL)) {
            
            if (strlen(ssid->valuestring) == 0) {
              cJSON_AddBoolToObject(res, "success", false);
              cJSON_AddStringToObject(res, "error", "SSID cannot be blank");
              resp_arg->res = cJSON_Print(res);
              break;
            }

            if (strlen(pass->valuestring) < 8) {
              cJSON_AddBoolToObject(res, "success", false);
              cJSON_AddStringToObject(res, "error", "PASS cannot be blank and must be longer than 8 characters");
              resp_arg->res = cJSON_Print(res);
              break;
            }

            if (strncpy(WIFI_SSID, ssid->valuestring, 32) != 0 && strncpy(WIFI_PASS, pass->valuestring, 32) != 0) {
              esp_err_t ret = write_string_to_nvs("wifi_ssid", ssid->valuestring);
              ret = write_string_to_nvs("wifi_pass", pass->valuestring);
              if (ret != ESP_OK) {
                cJSON_AddBoolToObject(res, "success", false);
                cJSON_AddStringToObject(res, "error", esp_err_to_name(ret));
                resp_arg->res = cJSON_Print(res);
                break;
              } else {
                cJSON_AddBoolToObject(res, "success", true);
                resp_arg->res = cJSON_Print(res);
                break;
              }
            } else {
              cJSON_AddBoolToObject(res, "success", false);
              cJSON_AddStringToObject(res, "error", "Failed to set wifi");
              resp_arg->res = cJSON_Print(res);
              break;
            }
          }

          cJSON_AddBoolToObject(res, "success", false);
          cJSON_AddStringToObject(res, "error", "Invalid Params");
          resp_arg->res = cJSON_Print(res);
          break;
        }
        case WS_OPT_REBOOT: {
          cJSON *value = cJSON_GetObjectItem(root, "value");
          if (cJSON_IsNumber(value) && value != NULL && value->valueint > 9000) {
            esp_restart();
          }
          
          cJSON_AddBoolToObject(res, "success", false);
          cJSON_AddStringToObject(res, "error", "Invalid Value");
          resp_arg->res = cJSON_Print(res);
          break;
        }
        case WS_OPT_GATTATTACK: {
          cJSON *item = cJSON_GetObjectItem(root, "action");
          if (cJSON_IsString(item) && item->valuestring != NULL) {
            GattAttackWebEventParams ps;

            ps.action = item->valuestring;

            cJSON *id = cJSON_GetObjectItem(root, "id");
            if (cJSON_IsNumber(id) && id != NULL) {
              ps.id = id->valueint;
            }

            cJSON *hook_ret = cJSON_GetObjectItem(root, "hook_ret");
            if (cJSON_IsString(hook_ret) && hook_ret->valuestring != NULL) {
              ps.hook_ret = hook_ret->valuestring;
            }

            cJSON *hook_enable = cJSON_GetObjectItem(root, "enable");
            if (cJSON_IsBool(hook_enable)) {
              ps.enable = cJSON_IsTrue(hook_enable);
            }

            GattAttackApp::webEvent(&ps);

            return ESP_OK;
          } else {
            cJSON_AddBoolToObject(res, "success", false);
            cJSON_AddStringToObject(res, "error", "Invalid Action");
            resp_arg->res = cJSON_Print(res);
          }
          break;
        }
        default:
          cJSON_AddBoolToObject(res, "success", false);
          cJSON_AddStringToObject(res, "error", "Invalid Option");
          resp_arg->res = cJSON_Print(res);
          break;
      }
    } else {
      cJSON_AddBoolToObject(res, "success", false);
      cJSON_AddStringToObject(res, "error", "Invalid Option");
      resp_arg->res = cJSON_Print(res);
    }

    cJSON_Delete(root);
    free(buf);

    if (resp_arg->res != NULL && httpd_queue_work(req->handle, ws_async_send, resp_arg) != ESP_OK) {
      free(resp_arg->res);
      free(resp_arg);
    }

    cJSON_Delete(res);
    return ESP_OK;
  }

  ret = httpd_ws_send_frame(req, &ws_pkt);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "httpd_ws_send_frame failed with %d", ret);
  }

  free(buf);
  return ret;
}
