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
