#include "mbed.h"
#include "FreeRTOS.h"
#include "task.h"
#include "hitbit.h"
#include "shared.h"

#define HITBIT_PERIOD 200       /* ms between LED rotations */
#define HITBIT_START  0b0101    /* first pattern of a game: LED1 and LED3 */
#define LED4_BIT      0b1000    /* bit of LED4 in the pattern (bit 0 = LED1) */

TaskHandle_t hitbit_task;

static BusOut leds(LED1, LED2, LED3, LED4);    /* bit 0 = LED1 ... bit 3 = LED4 */

/* Move every bit one LED toward LED4; the bit on LED4 wraps around to LED1. */
static uint8_t rotate(uint8_t pattern)
{
    bool wrap = (pattern & LED4_BIT) != 0;

    pattern = (pattern << 1) & 0b1111;
    if (wrap)
        pattern |= 0b0001;
    return pattern;
}

/*-------------------------------------------------------------------------+
| Task: hitbit - Hit Bit game (Part 3: Simão Silva)
| Rotates the bit pattern across LED1-4 every period.
+--------------------------------------------------------------------------*/
void vHitBitTask(void *pvParameters)
{
    config_t c;
    uint8_t pattern = HITBIT_START;
    TickType_t last_wake = xTaskGetTickCount();     /* start of the current period */

    for (;;) {
        config_get(&c);
        if (!c.hb) {                    /* turned off: clean up, then stop here */
            leds = 0;
            pattern = HITBIT_START;     /* adhb 1 starts a new game */
            vTaskSuspend(NULL);         /* adhb 1 resumes the task from this line */
            last_wake = xTaskGetTickCount();    /* restart the rotation from now */
            continue;
        }
        leds = pattern;
        vTaskDelayUntil(&last_wake, HITBIT_PERIOD);
        pattern = rotate(pattern);
    }
}
