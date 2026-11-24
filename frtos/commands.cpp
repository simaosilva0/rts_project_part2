#include <stdio.h>
#include <stdlib.h>

#include "clock.h"
#include "lcd.h"
#include "shared.h"
#include "buzzer.h"
#include "bubble.h"
#include "hitbit.h"

/*-------------------------------------------------------------------------+
| Argument helpers
+--------------------------------------------------------------------------*/

/* Check that the command got exactly n arguments; print the usage if not. */
static int arg_count (int argc, int n, const char *usage)
{
  if (argc - 1 != n) {
    printf("usage: %s\n", usage);
    return 0;
  }
  return 1;
}

/* Convert s to an integer in [min, max]; print an error if it isn't one. */
static int arg_int (const char *s, const char *name, int min, int max, int *out)
{
  char *end;
  long v = strtol(s, &end, 10);

  if (end == s || *end != '\0' || v < min || v > max) {
    printf("%s must be a number %d..%d\n", name, min, max);
    return 0;
  }
  *out = (int)v;
  return 1;
}

/*-------------------------------------------------------------------------+
| Function: cmd_test - prints its arguments (parser check)
+--------------------------------------------------------------------------*/
void cmd_test (int argc, char** argv)
{
  int i;

  /* example: print the arguments */
  for (i=0; i<argc; i++)
    printf ("\nargv[%d] = %s", i, argv[i]);
}

/*-------------------------------------------------------------------------+
| Function: cmd_rp - read parameters (pmon, tala)
+--------------------------------------------------------------------------*/
void cmd_rp (int argc, char** argv)
{
  config_t c;

  config_get(&c);
  printf("pmon = %u s, tala = %u s\n", c.pmon, c.tala);
}

/*-------------------------------------------------------------------------+
| Function: cmd_todo - placeholder for commands not implemented yet
+--------------------------------------------------------------------------*/
void cmd_todo (int argc, char** argv)
{
  printf("%s: not implemented yet\n", argv[0]);
}

/*-------------------------------------------------------------------------+
| Function: cmd_mmp - modify monitoring period
+--------------------------------------------------------------------------*/
void cmd_mmp (int argc, char** argv)
{
  config_t c;
  int p;

  if (!arg_count(argc, 1, "mmp p") || !arg_int(argv[1], "p", 0, 99, &p))
    return;
  config_get(&c);
  c.pmon = p;
  config_set(&c);

  printf("pmon = %d s\n", p);
}

/*-------------------------------------------------------------------------+
| Function: cmd_mta - modify alarm duration
+--------------------------------------------------------------------------*/
void cmd_mta (int argc, char** argv)
{
  config_t c;
  int t;

  if (!arg_count(argc, 1, "mta t") || !arg_int(argv[1], "t", 0, 60, &t))
    return;
  config_get(&c);
  c.tala = t;
  config_set(&c);

  printf("tala = %d s\n", t);
}

/*-------------------------------------------------------------------------+
| Function: cmd_rai - read alarm info
+--------------------------------------------------------------------------*/
void cmd_rai (int argc, char** argv)
{
  config_t c;

  config_get(&c);
  printf("alarm clock: %02u:%02u:%02u (%s)\n",
         c.alah, c.alam, c.alas, c.alac ? "on" : "off");
  printf("temperature: tlow = %u C, thigh = %u C (%s)\n",
         c.tlow, c.thigh, c.alat ? "on" : "off");
}

/*-------------------------------------------------------------------------+
| Function: cmd_sac - set alarm clock
+--------------------------------------------------------------------------*/
void cmd_sac (int argc, char** argv)
{
  config_t c;
  int h, m, s;

  if (!arg_count(argc, 3, "sac h m s") ||
      !arg_int(argv[1], "h", 0, 23, &h) ||
      !arg_int(argv[2], "m", 0, 59, &m) ||
      !arg_int(argv[3], "s", 0, 59, &s))
    return;
  config_get(&c);
  c.alah = h;
  c.alam = m;
  c.alas = s;
  config_set(&c);

  printf("alarm clock = %02d:%02d:%02d\n", h, m, s);
}

/*-------------------------------------------------------------------------+
| Function: cmd_sat - set temperature thresholds
+--------------------------------------------------------------------------*/
void cmd_sat (int argc, char** argv)
{
  config_t c;
  int tl, th;

  if (!arg_count(argc, 2, "sat tl th") ||
      !arg_int(argv[1], "tl", 0, 50, &tl) ||
      !arg_int(argv[2], "th", 0, 50, &th))
    return;
  if (tl >= th) {
    printf("tl must be lower than th\n");
    return;
  }
  config_get(&c);
  c.tlow = tl;
  c.thigh = th;
  config_set(&c);

  printf("tlow = %d C, thigh = %d C\n", tl, th);
}

/*-------------------------------------------------------------------------+
| Function: cmd_adac - alarm clock on/off
+--------------------------------------------------------------------------*/
void cmd_adac (int argc, char** argv)
{
  config_t c;
  int on;

  if (!arg_count(argc, 1, "adac 1/0") || !arg_int(argv[1], "value", 0, 1, &on))
    return;
  config_get(&c);
  c.alac = on;
  config_set(&c);
  lcd_show_alarms(c.alac, c.alat);

  printf("alarm clock %s\n", on ? "on" : "off");
}

/*-------------------------------------------------------------------------+
| Function: cmd_adat - temperature alarm on/off
+--------------------------------------------------------------------------*/
void cmd_adat (int argc, char** argv)
{
  config_t c;
  int on;

  if (!arg_count(argc, 1, "adat 1/0") || !arg_int(argv[1], "value", 0, 1, &on))
    return;
  config_get(&c);
  c.alat = on;
  config_set(&c);
  lcd_show_alarms(c.alac, c.alat);

  printf("temperature alarm %s\n", on ? "on" : "off");
}

/*-------------------------------------------------------------------------+
| Function: cmd_rdt - read date/time
+--------------------------------------------------------------------------*/
void cmd_rdt (int argc, char** argv)
{
  struct tm now;
  char buf[20];

  clock_now(&now);
  strftime(buf, sizeof(buf), "%d/%m/%Y %H:%M:%S", &now);

  printf("%s\n", buf);
}

/*-------------------------------------------------------------------------+
| Function: cmd_rc - read clock
+--------------------------------------------------------------------------*/
void cmd_rc (int argc, char** argv)
{
  struct tm now;
  char buf[9];

  clock_now(&now);
  strftime(buf, sizeof(buf), "%H:%M:%S", &now);

  printf("%s\n", buf);
}

/*-------------------------------------------------------------------------+
| Function: cmd_sd - set date
+--------------------------------------------------------------------------*/
void cmd_sd (int argc, char** argv)
{
  int d, M, Y;

  if (!arg_count(argc, 3, "sd d M Y") ||
      !arg_int(argv[1], "d", 1, 31, &d) ||
      !arg_int(argv[2], "M", 1, 12, &M) ||
      !arg_int(argv[3], "Y", 1970, 2037, &Y))
    return;

  if (clock_set_date(d, M, Y) != pdPASS) {
    printf("%02d/%02d/%04d does not exist\n", d, M, Y);
    return;
  }

  printf("date = %02d/%02d/%04d\n", d, M, Y);
}

/*-------------------------------------------------------------------------+
| Function: cmd_sc - set clock
+--------------------------------------------------------------------------*/
void cmd_sc (int argc, char** argv)
{
  int h, m, s;

  if (!arg_count(argc, 3, "sc h m s") ||
      !arg_int(argv[1], "h", 0, 23, &h) ||
      !arg_int(argv[2], "m", 0, 59, &m) ||
      !arg_int(argv[3], "s", 0, 59, &s))
    return;
  clock_set_time(h, m, s);

  printf("clock = %02d:%02d:%02d\n", h, m, s);
}

/*-------------------------------------------------------------------------+
| Function: cmd_rts - read task state
+--------------------------------------------------------------------------*/
void cmd_rts (int argc, char** argv)
{
  config_t c;

  config_get(&c);
  printf("bubble level: %s, Hit Bit: %s, config sound: %s\n",
         c.bl ? "on" : "off", c.hb ? "on" : "off", c.cs ? "on" : "off");
}

/*-------------------------------------------------------------------------+
| Function: cmd_adbl - bubble level on/off
+--------------------------------------------------------------------------*/
void cmd_adbl (int argc, char** argv)
{
  config_t c;
  int on;

  if (!arg_count(argc, 1, "adbl 1/0") || !arg_int(argv[1], "value", 0, 1, &on))
    return;
  config_get(&c);
  c.bl = on;
  config_set(&c);
  if (on)
    vTaskResume(bubble_task);       /* off: the task suspends itself */

  printf("bubble level %s\n", on ? "on" : "off");
}

/*-------------------------------------------------------------------------+
| Function: cmd_adhb - Hit Bit on/off
+--------------------------------------------------------------------------*/
void cmd_adhb (int argc, char** argv)
{
  config_t c;
  int on;

  if (!arg_count(argc, 1, "adhb 1/0") || !arg_int(argv[1], "value", 0, 1, &on))
    return;
  config_get(&c);
  c.hb = on;
  config_set(&c);
  if (on)
    vTaskResume(hitbit_task);       /* off: the task suspends itself */

  printf("Hit Bit %s\n", on ? "on" : "off");
}

/*-------------------------------------------------------------------------+
| Function: cmd_adcs - config sound on/off
+--------------------------------------------------------------------------*/
void cmd_adcs (int argc, char** argv)
{
  config_t c;
  int on;

  if (!arg_count(argc, 1, "adcs 1/0") || !arg_int(argv[1], "value", 0, 1, &on))
    return;
  config_get(&c);
  c.cs = on;
  config_set(&c);
  buzzer_mode_changed();

  printf("config sound %s\n", on ? "on" : "off");
}
