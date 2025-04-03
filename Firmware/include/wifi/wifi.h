#ifndef WIFI_H
#define WIFI_H

#include "esp_wifi.h"
#include "lwip/inet.h"

#include "settings.h"

void wifi_init_softap(void);

#endif