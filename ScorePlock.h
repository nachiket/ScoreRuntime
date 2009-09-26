#ifndef _ScorePlock_h__
#define _ScorePlock_h__

// Nachiket disables PLOCKS..
#undef PLOCK

#if PLOCK
#include <sysmp.h>
#endif

/* these are the processor IDs where each of the {scheduler,
   simulator, user code} must run */
#define PLOCK_SCHEDULER_CPU       0
#define PLOCK_SIMULATOR_CPU       1
#define PLOCK_USER_CPU            0

void set_proc(int id, const char *str = 0);

#endif
