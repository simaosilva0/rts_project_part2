#ifndef BUZZER_H
#define BUZZER_H

#include "FreeRTOS.h"

typedef enum { ALARM_CLOCK, ALARM_TEMP } alarm_src_t;

BaseType_t buzzer_init(void);           /* call once in main(), before the scheduler */
void buzzer_alarm(alarm_src_t src);     /* ask for an alarm sound (tala s); never blocks */
void buzzer_mode_changed(void);         /* adcs: config sound turned on/off; never blocks */

void vBuzzerTask(void *pvParameters);   /* sounds the alarms (Part 2) */

#endif
