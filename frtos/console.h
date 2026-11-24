#ifndef CONSOLE_H
#define CONSOLE_H

#include "FreeRTOS.h"

BaseType_t console_init(void);          /* call once in main(), before the scheduler */
void vConsoleTask(void *pvParameters);  /* runs the command interpreter */

#endif