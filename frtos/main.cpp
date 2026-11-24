#include "mbed.h"
#include "FreeRTOS.h"
#include "task.h"
#include "shared.h"
#include "buzzer.h"
#include "console.h"
#include "clock.h"
#include "i2c_bus.h"
#include "lcd.h"
#include "temp.h"
#include "bubble.h"
#include "hitbit.h"

/* Task priorities (0 = idle, 4 = highest) and stack sizes (words) */
#define PRIO_HITBIT    4
#define PRIO_BUBBLE    3
#define PRIO_BUZZER    3
#define PRIO_CLOCK     3
#define PRIO_TEMP      2
#define PRIO_LCD       1
#define PRIO_CONSOLE   1

#define STACK_SMALL    (2*configMINIMAL_STACK_SIZE)
#define STACK_PRINTF   (4*configMINIMAL_STACK_SIZE)    /* tasks that call printf */

/* Create a task, or stop with a message if the heap is full. */
static void create_task(TaskFunction_t fn, const char *name, uint16_t stack,
                        UBaseType_t prio, TaskHandle_t *handle)
{
    if (xTaskCreate(fn, name, stack, NULL, prio, handle) != pdPASS) {
        printf("cannot create task %s: out of heap\n", name);
        for (;;);
    }
}

int main(void)
{
    if (console_init() != pdPASS || config_init() != pdPASS || clock_init() != pdPASS || lcd_init() != pdPASS ||
        i2c_bus_init() != pdPASS || buzzer_init() != pdPASS) {
        printf("init failed: out of heap\n");
        for (;;);
    }

    create_task(vConsoleTask, "console", STACK_PRINTF, PRIO_CONSOLE, NULL);
    create_task(vLcdTask,     "lcd",     STACK_SMALL,  PRIO_LCD,     NULL);
    create_task(vClockTask,   "clock",   STACK_SMALL,  PRIO_CLOCK,   NULL);
    create_task(vTempTask,    "temp",    STACK_SMALL,  PRIO_TEMP,    NULL);
    create_task(vBuzzerTask,  "buzzer",  STACK_SMALL,  PRIO_BUZZER,  NULL);
    create_task(vBubbleTask,  "bubble",  STACK_SMALL,  PRIO_BUBBLE,  &bubble_task);
    create_task(vHitBitTask,  "hitbit",  STACK_SMALL,  PRIO_HITBIT,  &hitbit_task);

    vTaskStartScheduler();

    /* Only reached if there was not enough heap to start the scheduler. */
    for (;;);
}
