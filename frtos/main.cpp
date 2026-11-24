#include "mbed.h"
#include "FreeRTOS.h"
#include "task.h"
#include "shared.h"
#include "console.h"
#include "clock.h"

int main(void)
{
    if (console_init() != pdPASS || config_init() != pdPASS || clock_init() != pdPASS) {
        printf("init failed: out of heap\n");
        for (;;);
    }

    xTaskCreate(vConsoleTask, "console", 4*configMINIMAL_STACK_SIZE, NULL, 1, NULL);

    vTaskStartScheduler();

    /* Only reached if there was not enough heap to start the scheduler. */
    for (;;);
}