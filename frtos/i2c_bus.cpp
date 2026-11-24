#include "i2c_bus.h"
#include "semphr.h"

static SemaphoreHandle_t i2c_mutex;

BaseType_t i2c_bus_init(void)
{
    i2c_mutex = xSemaphoreCreateMutex();
    return (i2c_mutex != NULL) ? pdPASS : pdFAIL;
}

void i2c_lock(void)
{
    xSemaphoreTake(i2c_mutex, portMAX_DELAY);
}

void i2c_unlock(void)
{
    xSemaphoreGive(i2c_mutex);
}
