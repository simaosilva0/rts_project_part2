#include "buzzer.h"
#include "queue.h"
#include "task.h"

#define BUZZER_QUEUE_LEN 4
#define REQ_MODE         0xFF   /* not an alarm: config sound turned on/off */

static QueueHandle_t buzzer_queue;      /* uint8_t: an alarm_src_t or REQ_MODE */

BaseType_t buzzer_init(void)
{
    buzzer_queue = xQueueCreate(BUZZER_QUEUE_LEN, sizeof(uint8_t));
    return (buzzer_queue != NULL) ? pdPASS : pdFAIL;
}

static void send(uint8_t req)
{
    xQueueSend(buzzer_queue, &req, 0);      /* full: the task is already busy */
}

void buzzer_alarm(alarm_src_t src)
{
    send((uint8_t)src);
}

void buzzer_mode_changed(void)
{
    send(REQ_MODE);
}

/*-------------------------------------------------------------------------+
| Task: buzzer - sounds the alarms (Part 2: Isam Abbasi)
| Stub: takes each request and discards it. Part 2 replaces the body.
+--------------------------------------------------------------------------*/
void vBuzzerTask(void *pvParameters)
{
    uint8_t req;

    for (;;)
        xQueueReceive(buzzer_queue, &req, portMAX_DELAY);
}
