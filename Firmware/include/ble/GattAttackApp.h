#ifndef GATTATTACKAPP_H
#define GATTATTACKAPP_H

#include <string>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "cJSON.h"

#include "settings.h"
#include "LEDStrip.h"
#include "ble/ble.h"
#include "ble/GattAttack.h"
#include "wifi/ws.h"

enum GattAttackState {
  GATT_ATTACK_STATE_HOME,
  GATT_ATTACK_STATE_SCANNING,
  GATT_ATTACK_STATE_SCAN_RES,
  GATT_ATTACK_STATE_START,
  GATT_ATTACK_STATE_CLIENT_CONNECTING, // dont want to allow killing during this period to prevent crash
  GATT_ATTACK_STATE_CLIENT_CONNECT_FAIL,
  GATT_ATTACK_STATE_CLIENT_CONNECTED,
  GATT_ATTACK_STATE_RUNNING,
  GATT_ATTACK_STATE_FAILED,
};

struct GattAttackParams {
  uint8_t *target;
};

bool should_run_hooks();
char *run_hook(const char *type, const char *uuid, const char *value);
void send_output(std::string output, bool isJSON = false);

struct GattAttackWebEventParams {
  const char *action;
  int id;
  const char *hook_ret;
  bool enable;
};

class GattAttackApp {
  private:
    static void scan_task(void *taskParams);
    static void atk_task(void *taskParams);

  public:
    static void sendState(int state);
    static void webEvent(GattAttackWebEventParams *ps);
    static bool isRunning;
    static void scan();
    static void scan(void *params);
    static void stop_attack();
    static void start_attack(esp_bd_addr_t bd_target);
};

#endif // GATTATTACKAPP_H
