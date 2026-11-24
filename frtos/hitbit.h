#ifndef HITBIT_H
#define HITBIT_H

#include "FreeRTOS.h"
#include "task.h"

extern TaskHandle_t hitbit_task;        /* set by main(); adhb 1 resumes it */

void vHitBitTask(void *pvParameters);   /* Hit Bit game (Part 3) */

#endif
