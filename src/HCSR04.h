#ifndef __HCSR04__
#define __HCSR04__
/* 
  Note that HC-SR044 is a 5V device. Supply 5VDC to VCC, device can be triggered with 3V3
  Wire the echo pin via a voltage divider (1k, 220R) to ESP
*/
#include <Arduino.h>
#include "HampelFilter.h"

class HCSR04
{
  TimerHandle_t thandle = nullptr;
  unsigned period_ms;
  int trigger_pin, echo_pin;
  float distance_cm = 0;
  HampelFilter *hampel;
  SemaphoreHandle_t mutex;

public:
  HCSR04();
  esp_err_t begin(int trigger_pin, int echo_pin, unsigned period_ms = 1000);
  static void callback(TimerHandle_t th);
  float getValue();
};

#endif