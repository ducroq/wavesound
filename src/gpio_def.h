#ifndef __GPIO_DEF__
#define __GPIO_DEF__

// Hardware Connections LOLIN32
// #define I2C_SDA 21 // I2C_1 Data
// #define I2C_SCL 22 // I2C_1 Clock

#define I2S_BCLK 26     // I2S Clock
#define I2S_LRCK 27     // Left/Right audio
#define I2S_DIN 25      // I2S Data
#define PCM5102_XSMT 33 // Softmute(Low)/ softun-mute(High)

#define HCSR04_TRIG 18
#define HCSR04_ECHO 19

#define BUZZ 5 

// #define KY_040_1_CLK  28  // ky-040 clk pin,             add 100nF/0.1uF capacitors between pin & ground!!!
// #define KY_040_1_DATA 29  // ky-040 dt  pin,             add 100nF/0.1uF capacitors between pin & ground!!!
// #define KY_040_1_SW   5  // ky-040 sw  pin, interrupt & add 100nF/0.1uF capacitors between pin & ground!!!
#define KY_040_1_CLK  23  // ky-040 clk pin,             add 100nF/0.1uF capacitors between pin & ground!!!
#define KY_040_1_DATA 22  // ky-040 dt  pin,             add 100nF/0.1uF capacitors between pin & ground!!!
#define KY_040_1_SW   21  // ky-040 sw  pin, interrupt & add 100nF/0.1uF capacitors between pin & ground!!!

const uint8_t adc_pin_mapping[] =  {4, 0, 2, 15, 13, 12, 14, 36, 39, 32, 34, 35};



#endif