#include <Arduino.h>
#include "I2Ssynth.h"
#include "mutex.h"
#include "rng.h"
#include "gpio_def.h"
/*
 *  Inspired by
 *  https://github.com/schreibfaul1/ESP32-audioI2S
 *  http://mpgedit.org/mpgedit/mpeg_format/mpeghdr.htm
 *  https://docs.espressif.com/projects/esp-idf/en/latest/api-reference/peripherals/i2s.html
 *  https://github.com/earlephilhower/ESP8266Audio/blob/master/src/AudioOutputI2S.cpp
 *  https://www.isip.piconepress.com/projects/speech/software/tutorials/production/fundamentals/v1.0/section_02/s02_01_p05.html
 *  https://web.archive.org/web/20141213140451/https://ccrma.stanford.edu/courses/422/projects/WaveFormat/
 *  https://stackoverflow.com/questions/2031163/when-to-use-the-different-log-levels
 * 
 */

I2Ssynth::I2Ssynth()
{
    // Create mutex
    mutex = xSemaphoreCreateMutex();
    assert(mutex);
}

esp_err_t I2Ssynth::i2sConfig(int bclkPin, int lrckPin, int dinPin)
{
    // i2s configuration: Tx to ext DAC, 2's complement 16-bit PCM, mono,
    const i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX), // | I2S_CHANNEL_MONO), // only tx, external DAC
        .sample_rate = sample_rate,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        // .channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT, // single channel
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT, //2-channels
        .communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL3, // highest interrupt priority that can be handeled in c
        .dma_buf_count = 8,                      // 16,
        .dma_buf_len = 1024,                      //
        .use_apll = false,
        .tx_desc_auto_clear = true, // new in V1.0.1
        .fixed_mclk = -1};

    const i2s_pin_config_t pin_config = {
        .bck_io_num = bclkPin,           // this is BCK pin
        .ws_io_num = lrckPin,            // this is LRCK pin
        .data_out_num = dinPin,          // this is DATA output pin
        .data_in_num = I2S_PIN_NO_CHANGE // Not used
    };
    esp_err_t ret1 = i2s_driver_install((i2s_port_t)i2s_num, &i2s_config, 0, NULL);
    esp_err_t ret2 = i2s_set_pin((i2s_port_t)i2s_num, &pin_config);
    esp_err_t ret3 = i2s_set_sample_rates((i2s_port_t)i2s_num, sample_rate);

    return ret1 + ret2 + ret3;
}

esp_err_t I2Ssynth::begin(int bclkPin, int lrckPin, int dinPin)
{
    if (i2sConfig(bclkPin, lrckPin, dinPin) == ESP_FAIL)
    {
        printf("i2s driver error\n");
        return ESP_FAIL;
    }
    else
    {
        fill_noise_bank();
        printf("i2s driver configured\n");
        return ESP_OK;
    }
}

void I2Ssynth::fill_noise_bank()
{
    randomSeed(rng(analogRead(adc_pin_mapping[0])));
    for (uint16_t i = 0; i < noise_bank_size; i++)
        noise_bank[i] = random(-32768, +32767);
}

esp_err_t I2Ssynth::loop()
/*
    asynchrous sample output, should be done within 1/f_s s!
*/
{
    int32_t output[2] = {0, 0}; // 16 bits signed PCM assumed

    take_mutex(mutex);
    for (uint8_t track = 0; track < nr_of_tracks; track++)
    {
        if (pool[track].is_playing)
        {
            for (int8_t i = 0; i < 2; i++)
            // stereo, so two words
            {
                // get a noise sample
                pool[track].noise_it += (track + i + 1);
                if (pool[track].noise_it >= noise_bank_size)
                    pool[track].noise_it = rng(analogRead(adc_pin_mapping[track]));
                int32_t x = noise_bank[pool[track].noise_it];

                // filter noise
                // transposed direct form II implementation preferred since zeroes are implemented first
                int32_t y = x + pool[track].w[i];
                pool[track].w[i] = -(pool[track].coef_1000 * (x + y)) / 1000;
                y /= pool[track].invScaleValue;
                output[i] += pool[track].vol_1000 * y / 1000;
                // Serial.printf("%d, %d, %d, %d\n",x, pool[track].w[i], y, output[i]);
            }
        }
    }

    // set main volume
    for (int i = 0; i < 2; i++) {
        output[i] = vol_1000 * output[i] / 1000;
        output[i] = output[i] < 0xffff ? output[i] : 0xffff;
    }

    give_mutex(mutex);

    size_t i2s_bytes_written;
    uint32_t s32 = (output[1] << 16) | (output[0] & 0xffff);
    if (i2s_write((i2s_port_t)i2s_num, (const char *)&s32, sizeof(uint32_t), &i2s_bytes_written, portMAX_DELAY) == ESP_FAIL)
    {
        printf("Loop error in i2s_write.\n");
        return ESP_FAIL;
    }
    if (i2s_bytes_written != sizeof(uint32_t))
    {
        printf("Loop error, i2s_write has written %i bytes.\n", i2s_bytes_written);
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t I2Ssynth::setMainVolume(float val)
{
    if (val <= 1)
    {
        take_mutex(mutex);
        vol_1000 = int32_t(1000 * val);
        give_mutex(mutex);
        return ESP_OK;
    }
    else
        return ESP_FAIL;
}

int I2Ssynth::findFreeTrack()
{
    for (int track = 0; track < nr_of_tracks; track++)
    {
        if (!pool[track].is_playing)
            return track;
    }
    return -1;
}

esp_err_t I2Ssynth::playNoise(uint8_t track, float vol, float col)
/* 
 * Play noise sample on TRACK in pool at volume VOL and with color COL
 * Volume is defined between 0 and 1
 * Color is defined between 0 and 1, where 0 is light and 1 is dark
 */
{
    if ((track < nr_of_tracks) && (vol >= 0) && (vol <= 1) && (col >= 0) && (col <= 1))
    {
        take_mutex(mutex);
        pool[track].vol_1000 = int32_t(1000 * vol);
        pool[track].noise_it = random(noise_bank_size - 1);

        pool[track].coef_1000 = -(899 + 100 * col); // 900; //-700; //-971;
        pool[track].invScaleValue = (pool[track].coef_1000 * (1000 - pool[track].coef_1000)) / (1000 * (1000 + pool[track].coef_1000));
        pool[track].is_playing = true;
        give_mutex(mutex);

        return ESP_OK;
    }
    else
        return ESP_FAIL;
}

esp_err_t I2Ssynth::setTrackVolume(uint8_t track, float vol)
{
    if ((track < nr_of_tracks) && (vol >= 0) && (vol <= 1))
    {
        take_mutex(mutex);
        pool[track].vol_1000 = int32_t(1000 * vol);
        give_mutex(mutex);
        return ESP_OK;
    }
    else
        return ESP_FAIL;
}