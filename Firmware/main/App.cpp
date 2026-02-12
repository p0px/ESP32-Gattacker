#include "App.h"
#include "dns_server.h"

#define TAG "App"

void setup() {
  read_settings();
  wifi_init_softap();
  dns_server_config_t config = DNS_SERVER_CONFIG_SINGLE("*", "WIFI_AP_DEF");
  start_dns_server(&config);
  ws_init();
  start_rest_server();

  if (ap_mac_address[0] & 0x01) {
    ESP_LOGE(TAG, "AP Might not show due to ap_mac_address[0] & 0x01");
  }

  ESP_LOGI(TAG, "HTTP server started");
};
