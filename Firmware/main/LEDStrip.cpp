#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "LEDStrip.h"

#define TAG "leds"

TaskHandle_t ledTaskHandle = NULL;
float LEDStrip::brightness = 0.05;
bool LEDStrip::animationRunning = false;
Animations LEDStrip::currentState = BRIGHTNESS;

LEDStrip::LEDStrip() : led_strip(nullptr) {
  setup_leds();
}

LEDStrip::~LEDStrip() {
  if (led_strip) {
    led_strip_del(led_strip);
  }
}

void LEDStrip::setup_leds() {
  led_strip_config_t strip_config = {
    .strip_gpio_num = LED_STRIP_BLINK_GPIO,
    .max_leds = LED_STRIP_LED_NUMBERS,
    .led_model = LED_MODEL_WS2812,
    .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB
  };

  led_strip_rmt_config_t rmt_config = {
    .clk_src = RMT_CLK_SRC_DEFAULT,
    .resolution_hz = LED_STRIP_RMT_RES_HZ
  };

  ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
  ESP_LOGI(TAG, "Created LED strip object with RMT backend");
  vTaskDelay(1000 / portTICK_PERIOD_MS);
};

void LEDStrip::blink(int r, int g, int b, int times, int delay) {
  for (int i = 0; i < times; i++) {
    set_color(r, g, b);
    vTaskDelay(delay / portTICK_PERIOD_MS);
    set_color(0, 0, 0);
    vTaskDelay(delay / portTICK_PERIOD_MS);
    set_color(r, g, b);
    vTaskDelay(delay / portTICK_PERIOD_MS);
    set_color(0, 0, 0);
  }
};

void LEDStrip::set_color(int r, int g, int b) {
  for (int i = 0; i < LED_STRIP_LED_NUMBERS; i++) {
    set_color(i, r, g, b);
  }
};

void LEDStrip::set_color(int n, int r, int g, int b) {
  float scalingFactor = brightness;
  int scaledR = static_cast<int>(r * scalingFactor);
  int scaledG = static_cast<int>(g * scalingFactor);
  int scaledB = static_cast<int>(b * scalingFactor);
  esp_err_t ret = led_strip_set_pixel(led_strip, n, scaledG, scaledR, scaledB);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "set-color err: %s", esp_err_to_name(ret));
    setup_leds();
  }
  ret = led_strip_refresh(led_strip);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "refresh err: %s", esp_err_to_name(ret));
    setup_leds();
  }
};

void LEDStrip::set_brightness(float b) {
  brightness = b / float(100);
};

void LEDStrip::AnimationsTask(void* params) {
  LEDTaskParams* instance = static_cast<LEDTaskParams*>(params);

  while (1) {
    if (animationRunning) {
      switch (currentState) {
        case ANIMATION_NUM0: {
          instance->strip->set_brightness(2);
          for (int r = 0; r <= 255; r+=60) {
            for (int g = 0; g <= 255; g+=60) {
              for (int b = 0; b <= 255; b+=60) {
                instance->strip->set_color(0, r, g, b);
                
              }
            }
          }
          break;
        }
        case BRIGHTNESS: {
          for (int b = 10; b <= 100; b+=10) {
            instance->strip->set_brightness(b);
            instance->strip->set_color(0, 255, 0, 0);
            
            vTaskDelay(790 / portTICK_PERIOD_MS);
          }
          instance->strip->set_brightness(10);
          break;
        }
        case RAINBOW: {
          int delay = 150;
          instance->strip->set_brightness(10);
          for (int i = 0; i < 20; i++) {
            // Red
            instance->strip->set_color(0, 255, 0, 0);
            vTaskDelay(delay / portTICK_PERIOD_MS);

            // Orange
            instance->strip->set_color(0, 255, 165, 0);
            vTaskDelay(delay / portTICK_PERIOD_MS);

            // Yellow
            instance->strip->set_color(0, 255, 255, 0);
            vTaskDelay(delay / portTICK_PERIOD_MS);

            // Green
            instance->strip->set_color(0, 0, 255, 0);
            vTaskDelay(delay / portTICK_PERIOD_MS);

            // Blue
            instance->strip->set_color(0, 0, 0, 255);
            vTaskDelay(delay / portTICK_PERIOD_MS);

            // Indigo
            instance->strip->set_color(0, 75, 0, 130);
            vTaskDelay(delay / portTICK_PERIOD_MS);

            // Violet
            instance->strip->set_color(0, 150, 130, 150);
            vTaskDelay(delay / portTICK_PERIOD_MS);
          }
          break;
        }
        default:
          break;
      }

      instance->strip->next_animation();
    }
  }
};

void LEDStrip::next_animation() {
  currentState = static_cast<Animations>((currentState + 1) % ANIMATIONS_COUNT);
};

void LEDStrip::run_animations() {
  LEDTaskParams* params = new LEDTaskParams{ this };
  xTaskCreate(&LEDStrip::AnimationsTask, "LED animations task", 12000, params, 1, &ledTaskHandle);
  animationRunning = true;
};

void LEDStrip::stop_animations() {
  animationRunning = false;
  
  if (ledTaskHandle != NULL) {
    vTaskDelete(ledTaskHandle);
    ledTaskHandle = NULL;
  }

  set_color(0, 0, 0);
};

led_strip_t* LEDStrip::get_led_strip() const {
  return led_strip;
};
