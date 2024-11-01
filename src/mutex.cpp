#include "mutex.h"

void take_mutex(QueueHandle_t handle)
{
    BaseType_t rc;

    rc = xSemaphoreTake(handle, portMAX_DELAY);
    assert(rc == pdPASS);
}

void give_mutex(QueueHandle_t handle)
{
    BaseType_t rc;

    rc = xSemaphoreGive(handle);
    assert(rc == pdPASS);
}
