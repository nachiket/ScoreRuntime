/* AllocationTracker.cc */

#include <stdlib.h>
#include <memory.h>

#define ALLOCTRACKER_MAIN_FILE
#include "AllocationTracker.h"

#if TRACK_ALLOCATION

void *track_malloc(size_t s, char *fname, int lineno, char *funcname)
{
  void *ptr = malloc(s);
  REPORT_ALLOC(malloc,s,ptr,fname,lineno,funcname);

  return ptr;
}

void track_free(void *ptr, char *fname, int lineno, char *funcname)
{
  REPORT_FREE(free,ptr,fname,lineno,funcname);
  free(ptr);
}

#endif
