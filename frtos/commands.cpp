#include <stdio.h>
#include <stdlib.h>
#include "shared.h"

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