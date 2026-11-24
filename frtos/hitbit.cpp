#include "FreeRTOS.h"
#include "task.h"
#include "hitbit.h"
#include "shared.h"

#define HITBIT_PERIOD 200       /* ms between LED rotations */

TaskHandle_t hitbit_task;

/*-------------------------------------------------------------------------+
| Task: hitbit - Hit Bit game (Part 3: Simão Silva)
| Stub: does nothing while on. Part 3 adds the game.
+--------------------------------------------------------------------------*/
void vHitBitTask(void *pvParameters)
{
    config_t c;

    for (;;) {
        config_get(&c);
        if (!c.hb) {                    /* turned off: clean up, then stop here */
            /* Part 3: turn the 4 LEDs off */
            vTaskSuspend(NULL);         /* adhb 1 resumes the task from this line */
            continue;
        }
        /* Part 3: one game step */
        vTaskDelay(HITBIT_PERIOD);
    }
}
