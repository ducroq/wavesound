#include "HCSR04.h"
#include "mutex.h"

HCSR04::HCSR04()
// TODO, make pin assignment part of constructor and trigger period argument of begin function
{
  hampel = new HampelFilter(0.00, 5, 3.50);
  // Create mutex
  mutex = xSemaphoreCreateMutex();
  assert(mutex);
}

esp_err_t HCSR04::begin(int trigger_pin, int echo_pin, unsigned period_ms)
{
  this->trigger_pin = trigger_pin;
  this->echo_pin = echo_pin;
  this->period_ms = period_ms;
  pinMode(this->trigger_pin, OUTPUT);
  pinMode(this->echo_pin, INPUT);
  digitalWrite(this->trigger_pin, LOW);
  thandle = xTimerCreate(
      "HCSR04_tmr",
      pdMS_TO_TICKS(period_ms),
      pdTRUE,
      this,
      HCSR04::callback);
  assert(thandle);
  xTimerStart(thandle, portMAX_DELAY);
  printf("HCSR04 configured\n");
  return ESP_OK;
}

void HCSR04::callback(TimerHandle_t th)
{
  HCSR04 *obj = (HCSR04 *)pvTimerGetTimerID(th);
  const float alpha = .9;

  // Send ping:
  digitalWrite(obj->trigger_pin, HIGH);
  delayMicroseconds(10);
  digitalWrite(obj->trigger_pin, LOW);

  // Listen for echo:
  unsigned long usecs = pulseInLong(obj->echo_pin, HIGH, 50000);


  // Compute distance and do a bit of exponential smoothing
  if (usecs > 0 && usecs < 50000UL)
  {
    take_mutex(obj->mutex);
    obj->distance_cm = alpha * ((float)usecs) / 58.0 + (1 - alpha) * obj->distance_cm;
    obj->hampel->write(obj->distance_cm);
    give_mutex(obj->mutex);
  }
}

float HCSR04::getValue()
/* Take mutex and get value from Hampel filter, then give mutex */
{
  float val;

  take_mutex(mutex);
  // val = hampel->readMedian();
  val = distance_cm;
  give_mutex(mutex);

  return val;
}
