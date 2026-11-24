#ifndef CLOCK_H
#define CLOCK_H

#include <time.h>
#include "FreeRTOS.h"

BaseType_t clock_init(void);                     /* call once in main(), before the scheduler */
void clock_now(struct tm *now);                  /* current date and time */
BaseType_t clock_set_date(int d, int M, int Y);  /* pdFAIL if the date doesn't exist */
void clock_set_time(int h, int m, int s);

#endif
