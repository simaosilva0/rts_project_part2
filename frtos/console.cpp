#include "mbed.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "console.h"

#define RX_QUEUE_LEN  32        /* chars buffered between the ISR and the task */
#define UART_IRQ_PRIO 6         /* must be >= 5 to call FreeRTOS from the ISR */

static RawSerial pc(USBTX, USBRX);
static QueueHandle_t rx_queue;

extern void monitor(void);

/*-------------------------------------------------------------------------+
| UART0 receive interrupt: move every received char into rx_queue
+--------------------------------------------------------------------------*/
static void rx_isr(void)
{
    BaseType_t woken = pdFALSE;
    char c;

    while (pc.readable()) {
        c = pc.getc();                              /* reading clears the interrupt */
        xQueueSendFromISR(rx_queue, &c, &woken);    /* queue full: char is dropped */
    }
    portYIELD_FROM_ISR(woken);
}

BaseType_t console_init(void)
{
    pc.baud(115200);                    /* 115200 bit/s, must match the terminal */
    pc.format(8, SerialBase::None, 1);  /* 8N1: 8 data bits, no parity, 1 stop bit (the default) */

    rx_queue = xQueueCreate(RX_QUEUE_LEN, sizeof(char));
    if (rx_queue == NULL)
        return pdFAIL;

    NVIC_SetPriority(UART0_IRQn, UART_IRQ_PRIO);
    pc.attach(&rx_isr, SerialBase::RxIrq);
    return pdPASS;
}

/*-------------------------------------------------------------------------+
| Function: my_fgets        (called from my_getline / monitor)
+--------------------------------------------------------------------------*/
char* my_fgets (char* ln, int sz, FILE* f)
{
  int i = 0;
  char c;
  while (i < sz-1) {
      xQueueReceive(rx_queue, &c, portMAX_DELAY);   /* blocks until a char arrives */
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