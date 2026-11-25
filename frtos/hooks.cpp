#include "mbed.h"
#include "FreeRTOS.h"
#include "task.h"

/* Write straight to the UART registers: no printf, no interrupts, no
   C library, so it still works when the system is already broken. */
static void raw_puts(const char *s)
{
    while (*s) {
        while (!(LPC_UART0->LSR & (1 << 5)))    /* wait until the transmitter is free */
            ;
        LPC_UART0->THR = *s++;
    }
}

static void fatal(const char *what, const char *name)
{
    taskDISABLE_INTERRUPTS();
    raw_puts("\r\nFATAL: ");
    raw_puts(what);
    raw_puts(name);
    raw_puts("\r\n");
    for (;;);
}

/* FreeRTOS is C code, so its hooks must have C names (no C++ name mangling). */
extern "C" {

/* Called by the kernel when a task has gone past the end of its stack. */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    fatal("stack overflow in task ", pcTaskName);
}

/* Called by pvPortMalloc when the FreeRTOS heap is full. */
void vApplicationMallocFailedHook(void)
{
    fatal("out of heap", "");
}

}
