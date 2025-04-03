#include "App.h"

#define TAG "App"

LEDStrip* leds;

void setup() {
  read_settings();

  leds = new LEDStrip();
  leds->run_animations();

  wifi_init_softap();
  start_rest_server();

  if (ap_mac_address[0] & 0x01) {
    ESP_LOGE(TAG, "AP Might not show due to ap_mac_address[0] & 0x01");
  }

  ESP_LOGI(TAG, "HTTP server started");
};
