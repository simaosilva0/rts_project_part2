#include "mbed.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

DigitalOut led1(LED1);
DigitalOut led2(LED2);
Serial pc(USBTX, USBRX);

QueueHandle_t xQueue;

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

void vTask1( void *pvParameters ) {
int32_t lValueToSend;
BaseType_t xStatus;
    led1 = 1;
    for( ;; ) {
        lValueToSend = 201;
        xStatus = xQueueSend( xQueue, &lValueToSend, 0 );
        monitor(); //does not return
        led1 = !led1;
    }
}

void vTask2( void *pvParameters ) {
int32_t lReceivedValue;
BaseType_t xStatus;

    led2 = 1;
    printf("Hello from mbed -- FreeRTOS / cmd\n");
    for( ;; ) {
        vTaskDelay( 1000 );
        xStatus = xQueueReceive( xQueue, &lReceivedValue, 1000 );
        if( xStatus == pdPASS ) {
            printf( "\nReceived = %ld\n", (long)lReceivedValue );
        }
        led2 = !led2;
    }
}

int main( void ) {
    /* Perform any hardware setup necessary. */
//    prvSetupHardware();

    pc.baud(115200);
    pc.format(8, SerialBase::None, 1);

//    printf("Hello from mbed -- FreeRTOS / cmd\n");

    /* --- APPLICATION TASKS CAN BE CREATED HERE --- */

    xQueue = xQueueCreate( 3, sizeof( int32_t ) );
    xTaskCreate( vTask1, "Task 1", 4*configMINIMAL_STACK_SIZE, NULL, 1, NULL );
    xTaskCreate( vTask2, "Task 2", 4*configMINIMAL_STACK_SIZE, NULL, 2, NULL );
    /* Start the created tasks running. */
    vTaskStartScheduler();

    /* Execution will only reach here if there was insufficient heap to
    start the scheduler. */
    for( ;; );
    return 0;
}