#include "mbed.h"
#include "clock.h"
#include "semphr.h"

static SemaphoreHandle_t rtc_mutex;     /* protects the RTC registers */

BaseType_t clock_init(void)
{
    rtc_mutex = xSemaphoreCreateMutex();
    if (rtc_mutex == NULL)
        return pdFAIL;
    time(NULL);     /* starts the RTC at 01/01/1970 00:00:00 if it isn't running */
    return pdPASS;
}

/* time() reads the RTC registers one at a time, so a read that spans a
   second change can mix old and new fields. Two equal reads in a row can't. */
static time_t rtc_read_stable(void)
{
    time_t a, b = time(NULL);

    do {
        a = b;
        b = time(NULL);
    } while (a != b);
    return b;
}

static time_t rtc_get(void)
{
    time_t t;

    xSemaphoreTake(rtc_mutex, portMAX_DELAY);
    t = rtc_read_stable();
    xSemaphoreGive(rtc_mutex);
    return t;
}

static void rtc_set(time_t t)
{
    xSemaphoreTake(rtc_mutex, portMAX_DELAY);
    set_time(t);
    xSemaphoreGive(rtc_mutex);
}

void clock_now(struct tm *now)
{
    time_t t = rtc_get();
    localtime_r(&t, now);
}

BaseType_t clock_set_date(int d, int M, int Y)
{
    struct tm tm;
    time_t t;

    clock_now(&tm);             /* keep the current time of day */
    tm.tm_mday = d;
    tm.tm_mon  = M - 1;         /* struct tm counts months 0..11 */
    tm.tm_year = Y - 1900;      /* and years since 1900 */
    t = mktime(&tm);            /* normalizes: 31/02 becomes 03/03 */
    if (t == (time_t)-1 || tm.tm_mday != d || tm.tm_mon != M - 1)
        return pdFAIL;
    rtc_set(t);
    return pdPASS;
}

void clock_set_time(int h, int m, int s)
{
    struct tm tm;

    clock_now(&tm);             /* keep the current date */
    tm.tm_hour = h;
    tm.tm_min  = m;
    tm.tm_sec  = s;
    rtc_set(mktime(&tm));
}
