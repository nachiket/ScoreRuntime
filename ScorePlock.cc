#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

#include "ScorePlock.h"


void set_proc(int id, const char *str) // =0
{
#if PLOCK
  // set the processor lock
  int ret;

  if (DEBUG || EXTRA_DEBUG) {
    ret = sysmp(MP_CURPROC);
    if (str) printf("%s:", str);
    printf(" before: CPU %d\n ", ret);
    fflush(stdout);
  }

  if ((ret = sysmp(MP_MUSTRUN, id)) < 0) {
    if (str) fprintf(stderr, "%s:", str);
    perror("set_proc");
    exit(1);
  }

  if (DEBUG || EXTRA_DEBUG) {
    ret = sysmp(MP_CURPROC);
    if (str) printf("%s:", str);
    printf(" after: CPU %d\n ", ret);
    fflush(stdout);
  }
#endif
}
