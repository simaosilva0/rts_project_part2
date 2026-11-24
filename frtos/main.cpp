#include "mbed.h"
#include "FreeRTOS.h"
#include "task.h"
#include "shared.h"

Serial pc(USBTX, USBRX);

extern void monitor(void);

/*-------------------------------------------------------------------------+
| Function: my_fgets        (called from my_getline / monitor)
+--------------------------------------------------------------------------*/
char* my_fgets (char* ln, int sz, FILE* f)
{
  int i = 0;
  char c;
  while (i < sz-1) {
      c = pc.getc();
      if (c == '\r' || c == '\n') {   /* Enter: end of line */
          printf("\n");
          break;
      }
      if (c == '\b' || c == 0x7f) {   /* backspace: drop the last char */
          if (i > 0) i--;
          continue;
      }
      if (c >= 32 && c < 127)         /* keep printable chars only */
          ln[i++] = c;
  }
  ln[i] = '\0';
  return ln;
}

/*-------------------------------------------------------------------------+
| Task: console - runs the command interpreter
+--------------------------------------------------------------------------*/
void vConsoleTask(void *pvParameters)
{
    monitor();      /* never returns */
}

int main(void)
{
    pc.baud(115200);                    /* 115200 bit/s, must match the terminal */
    pc.format(8, SerialBase::None, 1);  /* 8N1: 8 data bits, no parity, 1 stop bit (the default) */

    if (config_init() != pdPASS) {
        printf("config_init failed: out of heap\n");
        for (;;);
    }

    xTaskCreate(vConsoleTask, "console", 4*configMINIMAL_STACK_SIZE, NULL, 1, NULL);

    vTaskStartScheduler();

    /* Only reached if there was not enough heap to start the scheduler. */
    for (;;);
}
