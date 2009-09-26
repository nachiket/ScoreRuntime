/* AllocationTracker.h */

#ifndef AllocationTracker_h__
#define AllocationTracker_h__

#include <stdio.h>

#if TRACK_ALLOCATION

#define REPORT_FILE  stdout

#define REPORT_ALLOC(__msg__,__size__,__ptr__,__fname__,__lineno__,__funcname__) \
fprintf(REPORT_FILE, "%s: %d: %s <%s(%d/%d)>==>%u\n", \
__fname__, __lineno__, # __msg__, \
__funcname__, __size__,gAllocationTab += __size__,(unsigned int)__ptr__)

#define REPORT_FREE(__msg__,__ptr__,__fname__,__lineno__,__funcname__) \
fprintf(REPORT_FILE, "%s: %d: %s <%s(%u)>\n", __fname__, __lineno__, \
# __msg__, __funcname__,(unsigned int)__ptr__)



#ifdef ALLOCTRACKER_MAIN_FILE
size_t gAllocationTab = 0;

#else
extern size_t gAllocationTab;

#define malloc(__size__) track_malloc(__size__,__FILE__,__LINE__,__PRETTY_FUNCTION__)
#define free(__ptr__) track_free(__ptr__,__FILE__,__LINE__,__PRETTY_FUNCTION__)
#endif

void *track_malloc(size_t s, char *fname, int lineno, char *funcname);
void track_free(void *ptr, char *fname, int lineno, char *funcname);

#endif

#endif /* AllocationTracker_h__ */











