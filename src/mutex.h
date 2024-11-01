#ifndef __MUTEX__
#define __MUTEX__

#include <Arduino.h>

void take_mutex(QueueHandle_t handle);
void give_mutex(QueueHandle_t handle);

#endif