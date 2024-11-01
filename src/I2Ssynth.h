#ifndef __I2SSYNTH__
#define __I2SSYNTH__

#include <driver/i2s.h>

// const uint8_t 

struct Synth
{
  bool is_playing = false;
  int32_t vol_1000;

  // noise_bank iterator
  uint16_t noise_it = 0;

  // filter variables, increased bit-depth to accomodate computations
  int32_t coef_1000 = 0;
  int32_t invScaleValue = 0;
  int32_t w[2] = {0}; // for 2 audio channels
};

class I2Ssynth
{
private:
  const uint8_t i2s_num = I2S_NUM_0;             // i2s port number
  const uint16_t sample_rate = 22050; //44100;            // [Sps]
  static const uint16_t noise_bank_size = 22050; //44100; // samples
  static constexpr size_t nr_of_tracks = 10;
  SemaphoreHandle_t mutex;
  int32_t vol_1000;
  Synth pool[nr_of_tracks];
  int16_t noise_bank[noise_bank_size];

  esp_err_t i2sConfig(int bclkPin, int lrckPin, int dinPin);
  void fill_noise_bank();
  int findFreeTrack();

public:
  I2Ssynth();
  esp_err_t begin(int bclkPin, int lrckPin, int dinPin);
  esp_err_t loop();
  esp_err_t setMainVolume(float val);
  esp_err_t playNoise(uint8_t track, float vol, float col);
  esp_err_t setTrackVolume(uint8_t track, float vol);
};

#endif