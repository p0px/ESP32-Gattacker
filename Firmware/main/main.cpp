#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "App.h"

#define TAG "main"

extern "C" void app_main() {
  setup();

  while(true) {
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}