#include "buzzer.h"
#include "queue.h"

#define BUZZER_QUEUE_LEN 4

static QueueHandle_t buzzer_queue;

BaseType_t buzzer_init(void)
{
    buzzer_queue = xQueueCreate(BUZZER_QUEUE_LEN, sizeof(alarm_src_t));
    return (buzzer_queue != NULL) ? pdPASS : pdFAIL;
}

void buzzer_alarm(alarm_src_t src)
{
    xQueueSend(buzzer_queue, &src, 0);      /* full: an alarm is already waiting */
}
