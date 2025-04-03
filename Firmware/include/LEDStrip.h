#ifndef LEDSTRIP_H
#define LEDSTRIP_H

#include "led_strip.h"
#include "settings.h"

class LEDStrip;

struct LEDTaskParams {
  LEDStrip* strip;  // Include the controller instance
};

enum Animations {
  ANIMATION_NUM0, // 0-255
  BRIGHTNESS,
  RAINBOW,
  ANIMATIONS_COUNT
};

class LEDStrip {
private:
  led_strip_t *led_strip;
  static Animations currentState;
  static void AnimationsTask(void* params);

public:
  LEDStrip();  // Constructor
  ~LEDStrip(); // Destructor

  static bool animationRunning;
  static float brightness;
  led_strip_t* get_led_strip() const;

  void setup_leds();
  void blink(int r, int g, int b, int times = 1, int delay = 100);
  // sets all colors
  void set_color(int r, int g, int b);
  // sets single led color
  void set_color(int n, int r, int g, int b);
  void set_brightness(float b);
  void run_animations();
  void stop_animations();
  void next_animation();
};

#endif // LEDSTRIP_H
