#include "mbed.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "C12832.h"
#include "lcd.h"
#include "shared.h"

#define LCD_QUEUE_LEN 8

typedef enum { MSG_CLOCK, MSG_ALARMS, MSG_TEMP, MSG_BUBBLE, MSG_BUBBLE_OFF } msg_type_t;

typedef struct {
    msg_type_t type;
    union {                                 /* one of these, depending on type */
        struct { uint8_t h, m, s; } clock;
        struct { uint8_t c, t; } alarms;
        int16_t temp;                       /* tenths of C */
        struct { int8_t dx, dy; } bubble;
    };
} lcd_msg_t;

static C12832 lcd(p5, p7, p6, p8, p11);    /* mosi, sck, reset, a0, ncs */
static QueueHandle_t lcd_queue;

BaseType_t lcd_init(void)
{
    lcd_queue = xQueueCreate(LCD_QUEUE_LEN, sizeof(lcd_msg_t));
    return (lcd_queue != NULL) ? pdPASS : pdFAIL;
}

/*-------------------------------------------------------------------------+
| Sending side: called by other tasks
+--------------------------------------------------------------------------*/
static void send(const lcd_msg_t *msg)
{
    xQueueSend(lcd_queue, msg, 0);      /* full: drop, never block the caller */
}

void lcd_show_clock(int h, int m, int s)
{
    lcd_msg_t msg;
    msg.type = MSG_CLOCK;
    msg.clock.h = h;
    msg.clock.m = m;
    msg.clock.s = s;
    send(&msg);
}

void lcd_show_alarms(int clock_on, int temp_on)
{
    lcd_msg_t msg;
    msg.type = MSG_ALARMS;
    msg.alarms.c = clock_on;
    msg.alarms.t = temp_on;
    send(&msg);
}

void lcd_show_temp(int tenths)
{
    lcd_msg_t msg;
    msg.type = MSG_TEMP;
    msg.temp = tenths;
    send(&msg);
}

void lcd_show_bubble(int dx, int dy)
{
    lcd_msg_t msg;
    msg.type = MSG_BUBBLE;
    msg.bubble.dx = dx;
    msg.bubble.dy = dy;
    send(&msg);
}

void lcd_hide_bubble(void)
{
    lcd_msg_t msg;
    msg.type = MSG_BUBBLE_OFF;
    send(&msg);
}

/*-------------------------------------------------------------------------+
| Drawing side: only the LCD task runs these
+--------------------------------------------------------------------------*/

/* Draw a string at (x, y) in the frame buffer. */
static void draw_text(int x, int y, const char *s)
{
    lcd.locate(x, y);
    while (*s)
        lcd._putc(*s++);
}

/* Clear one text line on the left part of the screen. */
static void clear_line(int y)
{
    lcd.fillrect(0, y, LCD_BUBBLE_X - 2, y + 8, 0);
}

static int clamp(int v, int max)
{
    return (v > max) ? max : (v < -max) ? -max : v;
}

static void draw(const lcd_msg_t *msg)
{
    char buf[16];
    int t;

    switch (msg->type) {
    case MSG_CLOCK:
        snprintf(buf, sizeof(buf), "%02d:%02d:%02d",
                 msg->clock.h, msg->clock.m, msg->clock.s);
        clear_line(LCD_LINE0);
        draw_text(0, LCD_LINE0, buf);
        break;

    case MSG_ALARMS:
        clear_line(LCD_LINE1);
        draw_text(0, LCD_LINE1, "A:");
        if (msg->alarms.c) draw_text(14, LCD_LINE1, "C");
        if (msg->alarms.t) draw_text(24, LCD_LINE1, "T");
        break;

    case MSG_TEMP:
        t = msg->temp;
        snprintf(buf, sizeof(buf), "T(C)= %s%d.%d",
                 (t < 0) ? "-" : "", abs(t) / 10, abs(t) % 10);
        clear_line(LCD_LINE2);
        draw_text(0, LCD_LINE2, buf);
        break;

    case MSG_BUBBLE:
        lcd.fillrect(LCD_BUBBLE_X, 0, 127, 31, 0);
        lcd.rect(LCD_BUBBLE_X, 0, 127, 31, 1);
        lcd.fillcircle(LCD_BUBBLE_X + 16 + clamp(msg->bubble.dx, LCD_BUBBLE_MAX),
                       16 + clamp(msg->bubble.dy, LCD_BUBBLE_MAX), 3, 1);
        break;

    case MSG_BUBBLE_OFF:
        lcd.fillrect(LCD_BUBBLE_X, 0, 127, 31, 0);
        break;
    }
}

/*-------------------------------------------------------------------------+
| Task: lcd - owns the LCD, draws what the other tasks send
+--------------------------------------------------------------------------*/
void vLcdTask(void *pvParameters)
{
    lcd_msg_t msg;
    config_t c;

    lcd.set_auto_up(0);         /* draw in the buffer, send it once */
    lcd.cls();
    draw_text(0, LCD_LINE0, "--:--:--");
    draw_text(0, LCD_LINE2, "T(C)= --.-");
    lcd.copy_to_lcd();

    config_get(&c);
    lcd_show_alarms(c.alac, c.alat);    /* first message: the initial alarm state */

    for (;;) {
        xQueueReceive(lcd_queue, &msg, portMAX_DELAY);     /* sleep until a message */
        do {
            draw(&msg);
        } while (xQueueReceive(lcd_queue, &msg, 0) == pdPASS);   /* drain the queue */
        lcd.copy_to_lcd();                                  /* one SPI transfer */
    }
}
