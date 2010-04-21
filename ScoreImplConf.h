//////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 1999 The Regents of the University of California 
// Permission to use, copy, modify, and distribute this software and
// its documentation for any purpose, without fee, and without a
// written agreement is hereby granted, provided that the above copyright 
// notice and this paragraph and the following two paragraphs appear in
// all copies. 
//
// IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO ANY PARTY FOR
// DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, INCLUDING
// LOST PROFITS, ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION,
// EVEN IF THE UNIVERSITY OF CALIFORNIA HAS BEEN ADVISED OF THE POSSIBILITY OF
// SUCH DAMAGE. 
//
// THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY WARRANTIES,
// INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
// AND FITNESS FOR A PARTICULAR PURPOSE. THE SOFTWARE PROVIDED HEREUNDER IS ON
// AN "AS IS" BASIS, AND THE UNIVERSITY OF CALIFORNIA HAS NO OBLIGATIONS TO
// PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS. 
//
//////////////////////////////////////////////////////////////////////////////
//
// BRASS source file
//
// SCORE runtime support
// $Revision: 2.4 $
//
//////////////////////////////////////////////////////////////////////////////
#ifndef _ScoreImplConf_H
#define _ScoreImplConf_H

// Nachiket fix
// Apparently this was already defined somewher...
// #define _GNU_SOURCE

#define VERSION    3.1

/***/
#define DYNAMIC_SCHEDULER 0
//#define LEDA_STL_ITERATORS 1



#define SCHED_VIRT_TIME_BREAKDOWN  1
#define SCHED_VIRT_TIME_BREAKDOWN_VERBOSE  1

/* NO_COST - reconfiguration cost */
#define NO_COST  0
/* NO_OVERHEAD - scheduling overhead */
#define NO_OVERHEAD  0

#define PARALLEL_TIME  1

// Nachiket kills profiling
#define DOPROFILING  0
#define DOPROFILING_SCHEDULECLUSTERS  0
#define DOPROFILING_VERBOSE  0

/*
  This will cause the profiled scheduleClusters() cycle to be substituted
  in as the "faked" scheduler time. This will automatically turn on some of the
  same sections as DOPROFILING. Thus, in the same manner,
  DOPROFILING_SCHEDULERCLUSTERS should not be turned on!
*/
#define SCHEDULECLUSTERS_FOR_SCHEDTIME  0

/*
 This will cause the profiled doSchedule() (minus getArrayStatus() and
 issueReconfigCommands() and performCleanup()) cycle to be substituted
 in as the "faked" scheduler time. This will automatically turn on some of the
 same sections as DOPROFILING. Thus, in the same manner,
 DOPROFILING should not be turned on!
*/
// Nachiket is killing this nonsense to avoid #error assert
#define DOSCHEDULE_FOR_SCHEDTIME  0

#define KEEPRECONFIGSTATISTICS  0

/*-----------------------------------------*/

#define FINDDEADLOCK_VERBOSE  0
#define TRACK_LAST_VISITED_NODE  0
#define TRACK_ALLOCATION  0
#define ENABLE_STALL_DETECT  0
#define STALL_DETECT_VERBOSE  0
#define VERBOSE_STREAM  0
#define RECONFIG_ACCT  1
#undef GET_FEEDBACK
#define VERBOSEDEBUG  0
#define DEBUG  0
#define EXTRA_DEBUG  0

// Nachiket set this to 0 to avoid ThreadCounter and perfctr
#define TIMEACC  0
#define USE_POLLING_STREAMS  1
#define PRINTSTATE  0
#define PLOCK 1

#define VISUALIZE_STATE 1
#define OPTIMIZE  0

#define ASPLOS2000  1
#define _64LUTPAGES  1
#define TONY_GRAPH  0
#define CHECK_CUSTOMBOUNDS  1
#define CUSTOM_VERBOSE  0
#define FRONTIERLIST_USEPRIORITY  0


#endif /* _ScoreImplConf_H */

