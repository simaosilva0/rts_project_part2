#include "FreeRTOS.h"
#include "task.h"
#include "temp.h"

/*-------------------------------------------------------------------------+
| Task: temp - temperature monitoring (Part 1: Simão Silveira)
| Stub: sleeps forever. Part 1 replaces the body.
+--------------------------------------------------------------------------*/
void vTempTask(void *pvParameters)
{
    for (;;)
        vTaskDelay(portMAX_DELAY);
}
