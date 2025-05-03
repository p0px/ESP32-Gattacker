#include "wifi/wifi.h"

#define TAG "wifi"

#if ESP_IDF_VERSION > ESP_IDF_VERSION_VAL(5, 4, 0)
  #define ENABLE_CAPTIVE_DHCP_OPTION
#endif

//https://github.com/espressif/esp-idf/blob/38628f98b9000ded87827dccbd8a5a1827392cfb/examples/protocols/http_server/captive_portal/main/main.c

#ifdef ENABLE_CAPTIVE_DHCP_OPTION
  /* Add DHCP option 114 to signal a captive portal */
  static void dhcp_set_captiveportal_url(void) {
    // get the IP of the access point to redirect to
    esp_netif_ip_info_t ip_info;
    esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_AP_DEF"), &ip_info);

    char ip_addr[16];
    inet_ntoa_r(ip_info.ip.addr, ip_addr, 16);
    ESP_LOGI(TAG, "Set up softAP with IP: %s", ip_addr);

    // turn the IP into a URI
    char* captiveportal_uri = (char*) malloc(32 * sizeof(char));
    assert(captiveportal_uri && "Failed to allocate captiveportal_uri");
    strcpy(captiveportal_uri, "http://");
    strcat(captiveportal_uri, ip_addr);

    // get a handle to configure DHCP with
    esp_netif_t* netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");

    // set the DHCP option 114
    ESP_ERROR_CHECK(esp_netif_dhcps_option(netif, ESP_NETIF_OP_SET, ESP_NETIF_CAPTIVEPORTAL_URI, captiveportal_uri, strlen(captiveportal_uri)));
  }
#endif

void wifi_init_softap(void) {
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_t *ap_netif = esp_netif_create_default_wifi_ap();

  ESP_ERROR_CHECK(esp_netif_dhcps_stop(ap_netif));

  #ifdef ENABLE_CAPTIVE_DHCP_OPTION
    dhcp_set_captiveportal_url();
  #endif

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));
  int channel = 3;

  wifi_config_t wifi_ap_config = {
    .ap = {
      .ssid_len = (uint8_t)strlen(WIFI_SSID),
      .channel = (uint8_t)channel,
      .authmode = WIFI_AUTH_WPA2_PSK,
      .max_connection = 2,
      .pmf_cfg = {
        .required = false,
      },
    },
  };

  strncpy((char*)wifi_ap_config.ap.ssid, WIFI_SSID, sizeof(wifi_ap_config.ap.ssid) - 1);
  strncpy((char*)wifi_ap_config.ap.password, WIFI_PASS, sizeof(wifi_ap_config.ap.password) - 1);
  
  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_ap_config));
  ESP_ERROR_CHECK(esp_wifi_start());

  // Set custom IP address
  esp_netif_ip_info_t ip_info;
  IP4_ADDR(&ip_info.ip, 1, 3, 3, 7);
  IP4_ADDR(&ip_info.gw, 1, 3, 3, 7);
  IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0);
  ESP_ERROR_CHECK(esp_netif_set_ip_info(ap_netif, &ip_info));

  // Start DHCP server
  ESP_ERROR_CHECK(esp_netif_dhcps_start(ap_netif));
};