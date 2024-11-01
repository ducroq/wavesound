/*
Synthesize wave sounds based on noise generators
Amplitude modulation by US ping sensor
*/
#include <Arduino.h>
#include "WiFi.h"
// #include <esp_wifi.h>

#include "gpio_def.h"
#include "I2Ssynth.h"
#include "HCSR04.h"

// Global objects
I2Ssynth i2ssynth = I2Ssynth();
HCSR04 ping = HCSR04();
QueueHandle_t val_queue, vol_queue;
float main_volume = .5; // should be atomic, change that in a future version

void buzzerTask(void *pvParameters)
{
  const TickType_t xDelay = 100 / portTICK_PERIOD_MS; // x [ms]
  const float vol_thres = .35;
  float r = 3.9;
  uint16_t ds = 10;
  float x = ((float)random(1000)) / 1000.0, vol = 0;
  uint16_t i = 0;

  pinMode(BUZZ, OUTPUT);
  digitalWrite(BUZZ, LOW);

  for (;;)
  {
    if (xQueueReceive(val_queue, &r, 0)) // Don't block if the queue is empty.
    {
      // printf("value = %f\n", r);
    }
    if (xQueueReceive(vol_queue, &vol, 0)) // Don't block if the queue is empty.
    {
      // ds = 100/(10*vol);
      // printf("volume = %f %d\n", vol, ds);
    }
    // logistic map to produce chaotic signal
    x = r * x * (1 - x);
    // printf("x: %1.2f\n", x);

    if ((r <= 2.0) || (vol < vol_thres))
    {
      digitalWrite(BUZZ, 0);
    }
    else
    {

      if (i % ds)
      {
        digitalWrite(BUZZ, x > 0.5);
      }
      else
      {
        digitalWrite(BUZZ, 0);
      }
      i++;
      vTaskDelay(xDelay);
    }
  }
}

void rotarySwitchTask(void *pvParameters)
{
  const TickType_t xDelay = 2 / portTICK_PERIOD_MS; // x [ms]
  const float delta_value = -.01, min_value = 0.0, max_value = 1.0;
  float value = main_volume;

  static int lastCLK = 0;
  static int currentCLK;
  static int lastDATA;
  bool lastButtonState = HIGH;
  unsigned long lastDebounceTime = 0;
  const unsigned long debounceDelay = 50; // Debounce time in ms

  pinMode(KY_040_1_CLK, INPUT_PULLUP);
  pinMode(KY_040_1_DATA, INPUT_PULLUP);
  pinMode(KY_040_1_SW, INPUT_PULLUP);

  lastCLK = digitalRead(KY_040_1_CLK);
  lastDATA = digitalRead(KY_040_1_DATA);

  for (;;)
  {
    currentCLK = digitalRead(KY_040_1_CLK);

    // If last and current state of CLK are different, then pulse occurred
    // React to only 1 state change to avoid double count
    if (currentCLK != lastCLK)
    {
      // If the DATA state is different than the CLK state then
      // the encoder is rotating counterclockwise
      if (digitalRead(KY_040_1_DATA) != currentCLK)
      {
        value += delta_value;
        if (value > max_value)
          value = max_value;
      }
      else
      {
        value -= delta_value;
        if (value < min_value)
          value = min_value;
      }
      // printf("value = %.1f\n", value);
      main_volume = value; // should be atomic, change that in a future version
      if (xQueueOverwrite(val_queue, &value) != pdTRUE)
      {
        printf("Queue error\n");
      }
    }
    lastCLK = currentCLK; // Store last CLK state

    // Handle button with debouncing
    int reading = digitalRead(KY_040_1_SW);
    if (reading != lastButtonState)
    {
      lastDebounceTime = millis();
    }

    if ((millis() - lastDebounceTime) > debounceDelay)
    {
      if (reading == LOW)
      { // Button is active LOW
        printf("Button pressed\n");
      }
    }
    lastButtonState = reading;

    vTaskDelay(xDelay);
  }
}

void pingSensorTask(void *pvParameters)
{
  const TickType_t xDelay = 10 / portTICK_PERIOD_MS; // x [ms]
  const float alpha = 1e-2;
  const float snr_alpha = .9;
  float vol = 0;
  float new_val, clip_val, noisy_new_val, val;
  uint16_t i = 0;

  ping.begin(HCSR04_TRIG, HCSR04_ECHO, 200);
  printf("pingSensorTask running on core %d\n", xPortGetCoreID());

  for (;;)
  {
    new_val = ping.getValue();

    // clip and normalize value
    clip_val = 100;
    new_val = new_val > clip_val ? clip_val : new_val;
    new_val /= clip_val;

    // invert, add a bit of noise and do exponential smoothing
    noisy_new_val = snr_alpha * (1 - new_val) + (1 - snr_alpha) * ((float)random(1000)) / 1000;
    vol = alpha * noisy_new_val + (1 - alpha) * vol;
    // t_vol = vol-.4;
    // t_vol = t_vol < 0 ? 0 : t_vol;
    if (i % 10)
    {
      if (xQueueOverwrite(vol_queue, &vol) != pdTRUE)
      {
        printf("Queue error\n");
      }
      i2ssynth.setMainVolume(main_volume * vol); // main_volume should be atomic, change that in a future version
    }
    // printf("%1.2f,%1.2f\n", new_val, main_volume * vol);
    i++;
    vTaskDelay(xDelay);
  }
}

void setup()
{

  Serial.begin(115200);
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);

  uint64_t chipid = ESP.getEfuseMac(); // chip ID equals MAC address(length: 6 bytes).
  printf("ESP32 rev: %d,  ID = %04X", ESP.getChipRevision(),
         (uint16_t)(chipid >> 32));   // print High 4 bytes
  printf("%08X\n", (uint32_t)chipid); // print Low 4bytes.
  printf("      Flash size: %u kB, speed: %u, mode: %u\n",
         ESP.getFlashChipSize() >> 10, ESP.getFlashChipSpeed(), ESP.getFlashChipMode());
  printf("      Sketch size: %u kB,  %u kB free \n", ESP.getSketchSize() >> 10, ESP.getFreeSketchSpace() >> 10);
  printf("MainTask running on core %d\n", xPortGetCoreID());

  val_queue = xQueueCreate(1, sizeof(float));
  vol_queue = xQueueCreate(1, sizeof(float));
  if ((val_queue == NULL) || (vol_queue == NULL))
  {
    Serial.println("Error creating the queue");
  }

  TaskHandle_t h1, h2, h3;

  BaseType_t rc1 = xTaskCreatePinnedToCore(
      pingSensorTask,
      "pingtsk",
      2048, // Stack size
      nullptr,
      1,   // Priority
      &h1, // Task handle
      0    // CPU
  );
  assert(rc1 == pdPASS);
  assert(h1);

  // BaseType_t rc2 = xTaskCreatePinnedToCore(
  //     buzzerTask,
  //     "buzztsk",
  //     2048, // Stack size
  //     nullptr,
  //     0,   // Priority
  //     &h2, // Task handle
  //     0    // CPU
  // );
  // assert(rc2 == pdPASS);
  // assert(h2);

  BaseType_t rc3 = xTaskCreatePinnedToCore(
      rotarySwitchTask,
      "rotarySwitchTask",
      2048, // Stack size
      nullptr,
      0,   // Priority
      &h3, // Task handle
      0    // CPU
  );
  assert(rc3 == pdPASS);
  assert(h3);

  i2ssynth.begin(I2S_BCLK, I2S_LRCK, I2S_DIN);
  i2ssynth.setMainVolume(main_volume);

  const int NN = 9;
  for (int i = 0; i < NN * NN; i++)
    i2ssynth.playNoise(i, ((float)random(50, 100 * NN)) / (100 * NN), ((float)random(500, 1000)) / 1000); // 100 / ((float)random(1, 100 * NN)), ((float)random(1000)) / 1000);
}

void loop()
{
  i2ssynth.loop(); // feed samples as fast as possible
}