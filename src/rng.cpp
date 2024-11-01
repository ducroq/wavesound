
#include "rng.h"

// create a random integer from 0 - 65535
uint16_t rng(uint16_t seed)
{
    static uint16_t y = 0;
    if (seed != 0)
        y += (seed && 0x1FFF); // seeded with a different number
    y ^= y << 2;
    y ^= y >> 7;
    y ^= y << 7;
    return (y);
}