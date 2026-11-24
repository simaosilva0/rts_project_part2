#include "mbed.h"
#include "FreeRTOS.h"
#include "task.h"
#include "shared.h"
#include "buzzer.h"
#include "console.h"
#include "clock.h"
#include "i2c_bus.h"
#include "lcd.h"

int main(void)
{
    if (console_init() != pdPASS || config_init() != pdPASS || clock_init() != pdPASS || lcd_init() != pdPASS ||
        i2c_bus_init() != pdPASS || buzzer_init() != pdPASS) {
        printf("init failed: out of heap\n");
        for (;;);
    }

    xTaskCreate(vClockTask,   "clock",   2*configMINIMAL_STACK_SIZE, NULL, 3, NULL);
    xTaskCreate(vConsoleTask, "console", 4*configMINIMAL_STACK_SIZE, NULL, 1, NULL);
    xTaskCreate(vLcdTask,     "lcd",     2*configMINIMAL_STACK_SIZE, NULL, 1, NULL);

    vTaskStartScheduler();

    /* Only reached if there was not enough heap to start the scheduler. */
    for (;;);
}
