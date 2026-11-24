#ifndef LCD_H
#define LCD_H

#include <stdint.h>
#include "FreeRTOS.h"

/* LCD layout (C12832, 128x32 pixels, Small_7 font: 9 px tall) */
#define LCD_LINE0      0        /* y of each text line */
#define LCD_LINE1      11
#define LCD_LINE2      22
#define LCD_BUBBLE_X   96       /* bubble level area: x 96..127, y 0..31 */
#define LCD_BUBBLE_MAX 12       /* max bubble offset from the centre, px */

BaseType_t lcd_init(void);          /* call once in main(), before the scheduler */
void vLcdTask(void *pvParameters);  /* the only code that touches the LCD */

/* Ask the LCD task to show something. They never block: if the LCD
   queue is full the update is dropped (the next one replaces it). */
void lcd_show_clock(int h, int m, int s);
void lcd_show_alarms(int clock_on, int temp_on);
void lcd_show_temp(int tenths);         /* temperature in tenths of C, e.g. 235 = 23.5 */
void lcd_show_bubble(int dx, int dy);   /* offset from the centre, px */
void lcd_hide_bubble(void);             /* clear the bubble area (adbl 0) */

#endif
