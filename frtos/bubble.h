#ifndef BUBBLE_H
#define BUBBLE_H

#include "FreeRTOS.h"
#include "task.h"

extern TaskHandle_t bubble_task;        /* set by main(); adbl 1 resumes it */

void vBubbleTask(void *pvParameters);   /* bubble level (Part 3) */

#endif
