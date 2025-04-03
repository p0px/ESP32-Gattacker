#ifndef ROUTES_H
#define ROUTES_H

#include <fcntl.h>
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_vfs.h"
#include "esp_netif.h"
#include "esp_littlefs.h"

#include "wifi/ws.h"
#include "settings.h"

esp_err_t start_rest_server();

#endif