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
// $Revision: 1.58 $
//
//////////////////////////////////////////////////////////////////////////////
#ifndef _ScoreConfig_H

#define _ScoreConfig_H

// Nachiket: Linux no longer includes this from kernel 2.6.25 onwards
// #include <asm/page.h>
#include <unistd.h>

#include "AllocationTracker.h"

// defines default hardware parameters.
#define SCORE_DEFAULT_NUMPHYSICALPAGES       16
#define SCORE_DEFAULT_NUMPHYSICALSEGMENTS    16
#if _64LUTPAGES
// Assuming 2Mbits.
#define SCORE_DEFAULT_PHYSICALSEGMENTSIZE    (2097152/8)
#else
#define SCORE_DEFAULT_PHYSICALSEGMENTSIZE    2097152
#endif

// defines the default length of a timeslice (in cycles).
#define SCORE_DEFAULT_TIMESLICE			250000

// defines how many cycles in a timeslice a page/segment must be stalled 
// before it is considered freeable.
// NOTE: It is best to define this in terms of SCORE_TIMESLICE.
#define SCORE_DEFAULT_STALL_THRESHOLD	(SCORE_DEFAULT_TIMESLICE*1/2)

// defines what ratio of the nodes in a cluster must be freeable (i.e.
// stalled) before a cluster can be considered freeable.
#define SCORE_DEFAULT_CLUSTERFREEABLE_RATIO	0.5

// added by Nachiket
#define PAGE_SIZE	getpagesize()

// this defines the allocation size of a physical memory segment (in bytes).
// NOTE: we define this to be equal to the size of a virtual page in Linux.
// NOTE: under the current Linux version, it should be 4096.
#define SCORE_ALLOCSIZE_MEMSEG          PAGE_SIZE
#define SCORE_ALLOCSIZE_MEMSEG_BITS     12

// this defines the amount of a virtual segment to load in (in bytes).
// this defines the size of a stitch buffer (in bytes).
// NOTE: we should define this in terms of SCORE_ALLOCSIZE_MEMSEG
#if _64LUTPAGES
#define SCORE_DEFAULT_DATASEGMENTBLOCK_LOADSIZE (SCORE_ALLOCSIZE_MEMSEG*32)
#else
#define SCORE_DEFAULT_DATASEGMENTBLOCK_LOADSIZE (SCORE_ALLOCSIZE_MEMSEG*128)
#endif

// this defines the size (in bytes) taken by various configurations and
// state bits.
#if _64LUTPAGES
// NOTE: We assume 32b/LUT state, N cycles (64b interface).
//                 128b/LUT config, 4N cycles (64b interface).
//                 64b/cycle primmem<->CMB transfer.
// FIX ME! HOW MANY INPUTS PER PAGE DO WE EXPECT POSSIBLE?
//         RIGHT NOW, I AM ASSUMING 8 INPUTS. (each is 64b wide).
#define SCORE_DEFAULT_PAGECONFIG_SIZE       ((200/8)*64)
#define SCORE_DEFAULT_PAGESTATE_SIZE        ((32/8)*64)
#define SCORE_DEFAULT_PAGEFIFO_SIZE         (8*256*(64/8))
#define SCORE_DEFAULT_MEMSEGFIFO_SIZE       (8*256*(64/8))
#else
// FIX ME! FOR THE SCOREBOARD, A PAGE HAS NO CONFIG, ONLY
//         STATE THAT REPRESENTS BOTH CONFIG/STATE.
#define SCORE_DEFAULT_PAGECONFIG_SIZE       50000
#define SCORE_DEFAULT_PAGESTATE_SIZE        50000
#define SCORE_DEFAULT_PAGEFIFO_SIZE         256
#define SCORE_DEFAULT_MEMSEGFIFO_SIZE       256
#endif

// this defines the amount of FIFO buffer space each input stream has
// (in number of tokens).
#define SCORE_INPUTFIFO_CAPACITY_ARRAY    256
#define SCORE_INPUTFIFO_CAPACITY          256
//#define SCORE_INPUTFIFO_CAPACITY          (1<<15)

// this defines the default number of "spare" segment stitch and stream
// stitch the scheduler will instantiate.
#define SCORE_INIT_SPARE_SEGMENTSTITCH      128
#define SCORE_INIT_SPARE_STREAMSTITCH       128


// the unit here is MHz
#define ARRAY_CLOCK_SPEED 200

#ifndef VERBOSEDEBUG
#define VERBOSEDEBUG 0
#endif

#ifndef DEBUG
#define DEBUG 0
#endif

#ifndef DOPROFILING
#define DOPROFILING 0
#endif

#ifndef USE_POLLING_STREAMS
#define USE_POLLING_STREAMS         0
#endif

#define SCORE_LIBRARY_PATH_ENV "SCORE_LIBRARY_PATH"
#define SCORE_FEEDBACK_DIR_ENV "SCORE_FEEDBACK_DIR"
#define SCORE_SCHEDULER_ID_FILE "/proc/schedulerid"
#define SCORE_INSTANTIATE_MESSAGE_TYPE 1
#define SCORE_CONSTRUCT_NAME "construct"
#define SCORE_AUTO_FEEDBACK_EXTENSION "fauto"

// FIX ME! THIS IS TO TEMPORARILY FAKE THE TIME IT TAKES TO MAKE A SCHEDULING
//         DECISION! THIS IS NECESSARILY INCORRECT! BUT WILL WORK FOR NOW!

#if SCHED_VIRT_TIME_BREAKDOWN && NO_OVERHEAD
#define SCORE_DEFAULT_FAKE_SCHEDULER_TIME	0
#else
#define SCORE_DEFAULT_FAKE_SCHEDULER_TIME         50000
#endif

#define STREAM_OVERHEAD 0

// this defines the number of consecutive deadlocks that must be detected
// before we kill a running design.
#define SCORE_DEFAULT_NUM_CONSECUTIVE_DEADLOCKS_TO_KILL    10

#if SCHED_VIRT_TIME_BREAKDOWN && NO_COST
#define SIM_DEFAULT_COST_GETARRAYSTATUS                 0
#define SIM_DEFAULT_COST_STARTPAGE                      0
#define SIM_DEFAULT_COST_STOPPAGE                       0
#define SIM_DEFAULT_COST_STARTSEGMENT                   0
#define SIM_DEFAULT_COST_STOPSEGMENT                    0
#define SIM_DEFAULT_COST_DUMPPAGESTATE       (0) 
#define SIM_DEFAULT_COST_DUMPPAGEFIFO                 0 
#define SIM_DEFAULT_COST_LOADPAGECONFIG     (0)
#define SIM_DEFAULT_COST_LOADPAGESTATE       (0) 
#define SIM_DEFAULT_COST_LOADPAGEFIFO     (0) 
#define SIM_DEFAULT_COST_GETSEGMENTPOINTERS             0
#define SIM_DEFAULT_COST_DUMPSEGMENTFIFO  (0)
#define SIM_DEFAULT_COST_SETSEGMENTCONFIGPOINTERS       0
#define SIM_DEFAULT_COST_CHANGESEGMENTMODE              0
#define SIM_DEFAULT_COST_CHANGESEGMENTTRAANDPBOANDMAX   0
#define SIM_DEFAULT_COST_RESETSEGMENTDONEFLAG           0
#define SIM_DEFAULT_COST_LOADSEGMENTFIFO  (0)
#define SIM_DEFAULT_COST_MEMXFERPRIMARYTOCMB (0)
#define SIM_DEFAULT_COST_MEMXFERCMBTOPRIMARY (0)
#define SIM_DEFAULT_COST_MEMXFERCMBTOCMB     (0)
#define SIM_DEFAULT_COST_CONNECTSTREAM                  0

#else

// Simulator default cost model.
#if _64LUTPAGES
// NOTE: We assume 32b/LUT state, N cycles (64b interface).
//                 128b/LUT config, 4N cycles (64b interface).
//                 64b/cycle primmem<->CMB transfer.
// FIX ME! HOW MANY INPUTS PER PAGE DO WE EXPECT POSSIBLE?
//         RIGHT NOW, I AM ASSUMING 8 INPUTS. (each is 64b wide).
#define SIM_DEFAULT_COST_GETARRAYSTATUS                 2
#define SIM_DEFAULT_COST_STARTPAGE                      2
#define SIM_DEFAULT_COST_STOPPAGE                       2
#define SIM_DEFAULT_COST_STARTSEGMENT                   2
#define SIM_DEFAULT_COST_STOPSEGMENT                    2
#define SIM_DEFAULT_COST_DUMPPAGESTATE       ((32/32)*64) 
#define SIM_DEFAULT_COST_DUMPPAGEFIFO       ((64/32)*256)
#define SIM_DEFAULT_COST_LOADPAGECONFIG     ((128/32)*64)
#define SIM_DEFAULT_COST_LOADPAGESTATE       ((32/32)*64) 
#define SIM_DEFAULT_COST_LOADPAGEFIFO       ((64/32)*256) 
#define SIM_DEFAULT_COST_GETSEGMENTPOINTERS             6
#define SIM_DEFAULT_COST_DUMPSEGMENTFIFO    ((64/32)*256)
#define SIM_DEFAULT_COST_SETSEGMENTCONFIGPOINTERS       6
#define SIM_DEFAULT_COST_CHANGESEGMENTMODE              3
#define SIM_DEFAULT_COST_CHANGESEGMENTTRAANDPBOANDMAX   3
#define SIM_DEFAULT_COST_RESETSEGMENTDONEFLAG           3
#define SIM_DEFAULT_COST_LOADSEGMENTFIFO    ((64/32)*256)
#define SIM_DEFAULT_COST_MEMXFERPRIMARYTOCMB (((float)1)/(64/8))
#define SIM_DEFAULT_COST_MEMXFERCMBTOPRIMARY (((float)1)/(64/8))
#define SIM_DEFAULT_COST_MEMXFERCMBTOCMB     (((float)1)/(64/8))
#define SIM_DEFAULT_COST_CONNECTSTREAM                  2
#else
#define SIM_DEFAULT_COST_GETARRAYSTATUS                 2
#define SIM_DEFAULT_COST_STARTPAGE                      2
#define SIM_DEFAULT_COST_STOPPAGE                       2
#define SIM_DEFAULT_COST_STARTSEGMENT                   2
#define SIM_DEFAULT_COST_STOPSEGMENT                    2
#define SIM_DEFAULT_COST_DUMPPAGESTATE               5000
#define SIM_DEFAULT_COST_DUMPPAGEFIFO                 256 
#define SIM_DEFAULT_COST_LOADPAGECONFIG              5000
#define SIM_DEFAULT_COST_LOADPAGESTATE               5000    
#define SIM_DEFAULT_COST_LOADPAGEFIFO                 256
#define SIM_DEFAULT_COST_GETSEGMENTPOINTERS             6
#define SIM_DEFAULT_COST_DUMPSEGMENTFIFO              256
#define SIM_DEFAULT_COST_SETSEGMENTCONFIGPOINTERS       6
#define SIM_DEFAULT_COST_CHANGESEGMENTMODE              3
#define SIM_DEFAULT_COST_CHANGESEGMENTTRAANDPBOANDMAX   3
#define SIM_DEFAULT_COST_RESETSEGMENTDONEFLAG           3
#define SIM_DEFAULT_COST_LOADSEGMENTFIFO              256
#define SIM_DEFAULT_COST_MEMXFERPRIMARYTOCMB         4000
#define SIM_DEFAULT_COST_MEMXFERCMBTOPRIMARY         4000
#define SIM_DEFAULT_COST_MEMXFERCMBTOCMB             2000 
#define SIM_DEFAULT_COST_CONNECTSTREAM                  2
#endif

#endif

#if ASPLOS2000
// sets the default number of page nodes in a CP.
#define SCORE_DEFAULT_NUM_PAGENODES_IN_CP               1;
#endif

extern unsigned int numPhysicalPages;
extern unsigned int numPhysicalSegments;
extern unsigned int physicalSegmentSize;
extern unsigned int SCORE_TIMESLICE;
extern unsigned int SCORE_FAKE_SCHEDULER_TIME;
extern unsigned int SCORE_STALL_THRESHOLD;
extern float SCORE_CLUSTERFREEABLE_RATIO;
extern unsigned int SCORE_DATASEGMENTBLOCK_LOADSIZE;
extern unsigned int SCORE_PAGECONFIG_SIZE;
extern unsigned int SCORE_PAGESTATE_SIZE;
extern unsigned int SCORE_PAGEFIFO_SIZE;
extern unsigned int SCORE_MEMSEGFIFO_SIZE;
extern unsigned int SCORE_NUM_CONSECUTIVE_DEADLOCKS_TO_KILL;
extern unsigned int SCORE_SEGMENTTABLE_LEVEL0SIZE;
extern unsigned int SCORE_SEGMENTTABLE_LEVEL1SIZE;
extern unsigned int SIM_COST_GETARRAYSTATUS;
extern unsigned int SIM_COST_STARTPAGE;
extern unsigned int SIM_COST_STOPPAGE;
extern unsigned int SIM_COST_STARTSEGMENT;
extern unsigned int SIM_COST_STOPSEGMENT;
extern unsigned int SIM_COST_DUMPPAGESTATE;
extern unsigned int SIM_COST_DUMPPAGEFIFO;
extern unsigned int SIM_COST_LOADPAGECONFIG;
extern unsigned int SIM_COST_LOADPAGESTATE;
extern unsigned int SIM_COST_LOADPAGEFIFO;
extern unsigned int SIM_COST_GETSEGMENTPOINTERS;
extern unsigned int SIM_COST_DUMPSEGMENTFIFO;
extern unsigned int SIM_COST_SETSEGMENTCONFIGPOINTERS;
extern unsigned int SIM_COST_CHANGESEGMENTMODE;
extern unsigned int SIM_COST_CHANGESEGMENTTRAANDPBOANDMAX;
extern unsigned int SIM_COST_RESETSEGMENTDONEFLAG;
extern unsigned int SIM_COST_LOADSEGMENTFIFO;
extern float SIM_COST_MEMXFERPRIMARYTOCMB;
extern float SIM_COST_MEMXFERCMBTOPRIMARY;
extern float SIM_COST_MEMXFERCMBTOCMB;
extern unsigned int SIM_COST_CONNECTSTREAM;
extern unsigned int SCORE_NUM_PAGENODES_IN_CP;

// used to page align block sizes to PAGE_SIZE.
#define SCORE_PAGE_ALIGN(x) \
   ((x) + (PAGE_SIZE - ((x) % PAGE_SIZE)))

// used to align bit widths to 8 bit boundaries.
#define SCORE_ALIGN_BITWIDTH_TO_8(x) \
   ((x) + (8 - ((x) % 8)))

// defines the sizes for the allocation blocks for the buddy system that
// implements memory management in array CMBs.
// NOTE: We attempt to guarantee that blocks are allocated on OS page
//       boundaries (i.e. 4096).
#define SCORE_DEFAULT_SEGMENTTABLE_LEVEL0SIZE \
   SCORE_PAGE_ALIGN(SCORE_DEFAULT_DATASEGMENTBLOCK_LOADSIZE + \
                    SCORE_DEFAULT_MEMSEGFIFO_SIZE)
#define SCORE_DEFAULT_SEGMENTTABLE_LEVEL1SIZE \
   SCORE_PAGE_ALIGN(SCORE_DEFAULT_PAGECONFIG_SIZE + \
                    SCORE_DEFAULT_PAGESTATE_SIZE + \
                    SCORE_DEFAULT_PAGEFIFO_SIZE)

// the bounds for custom data structures.
#define SCORE_SPARESEGMENTSTITCHLIST_BOUND                      1024
#define SCORE_SPARESTREAMSTITCHLIST_BOUND                       1024
#define SCORE_CLUSTERNODELIST_BOUND                             1024
#define SCORE_CLUSTERINPUTLIST_BOUND                            1024
#define SCORE_CLUSTEROUTPUTLIST_BOUND                           1024
#define SCORE_PROCESSOPERATORLIST_BOUND                         1024
#define SCORE_PROCESSNODELIST_BOUND                             1024
#define SCORE_PROCESSPROCESSORISTREAMLIST_BOUND                 1024
#define SCORE_PROCESSPROCESSOROSTREAMLIST_BOUND                 1024
#define SCORE_PROCESSCLUSTERLIST_BOUND                          1024
#define SCORE_PROCESSSTITCHBUFFERLIST_BOUND                     1024
#define SCORE_SCHEDULERDONENODECHECKLIST_BOUND                  1024
#define SCORE_SCHEDULERFRONTIERCLUSTERLIST_BOUND                1024
#define SCORE_SCHEDULERWAITINGCLUSTERLIST_BOUND                 1024
#define SCORE_SCHEDULERRESIDENTCLUSTERLIST_BOUND                1024
#define SCORE_SCHEDULERRESIDENTSTITCHLIST_BOUND                 1024
#define SCORE_SCHEDULERHEADCLUSTERLIST_BOUND                    1024
#define SCORE_SCHEDULERPROCESSORISTREAMLIST_BOUND               1024
#define SCORE_SCHEDULERPROCESSOROSTREAMLIST_BOUND               1024
#define SCORE_SCHEDULERDONENODELIST_BOUND                       1024
#define SCORE_SCHEDULERFREEABLECLUSTERLIST_BOUND                1024
#define SCORE_SCHEDULERDONECLUSTERLIST_BOUND                    1024
#define SCORE_SCHEDULERFAULTEDMEMSEGLIST_BOUND                  1024
#define SCORE_SCHEDULERADDEDBUFFERLOCKSTITCHBUFFERLIST_BOUND    1024
#define SCORE_SCHEDULERSCHEDULEDPAGELIST_BOUND                  1024
#define SCORE_SCHEDULERSCHEDULEDMEMSEGLIST_BOUND                1024
#define SCORE_SCHEDULERREMOVEDPAGELIST_BOUND                    1024
#define SCORE_SCHEDULERREMOVEDMEMSEGLIST_BOUND                  1024
#define SCORE_SCHEDULERDONENOTREMOVEDPAGELIST_BOUND             1024
#define SCORE_SCHEDULERDONENOTREMOVEDMEMSEGLIST_BOUND           1024
#define SCORE_SCHEDULERCONFIGCHANGEDSTITCHSEGLIST_BOUND         1024
#define SCORE_SCHEDULEREMPTYSTITCHLIST_BOUND                    1024
#define SCORE_SCHEDULERSTITCHBUFFERLIST_BOUND                   1024
#define SCORE_SCHEDULERPROCESSLIST_BOUND                        1024
#define SCORE_SCHEDULERNUMFREEPAGETRIAL_BOUND                   1024
#define SCORE_SCHEDULERNUMFREEMEMSEGTRIAL_BOUND                 1024
#define SCORE_SCHEDULERTRAVERSALTRIAL_BOUND                     1024
#define SCORE_SCHEDULERSCHEDULEDCLUSTERTRIAL_BOUND              1024
#define SCORE_SCHEDULERFRONTIERCLUSTERTRIAL_BOUND               1024
#define SCORE_SCHEDULERDEADLOCKEDPROCESSES_BOUND                1024
#define SCORE_SCHEDULEROPERATORSTREAMSFROMPROCESSOR_BOUND       1024
#define SCORE_SCHEDULEROPERATORSTREAMSTOPROCESSOR_BOUND         1024
#define SCORE_SCHEDULERUNUSEDPHYSICALCPS_BOUND                  1024
#define SCORE_SCHEDULERUNUSEDPHYSICALCMBS_BOUND                 1024

#define SCORE_SCHEDULERPROCESSORNODE_INPUT_BOUND                1024
#define SCORE_SCHEDULERPROCESSORNODE_OUTPUT_BOUND               1024 

#define SCORE_SCHEDULERSTITCHLISTTOMARKSCHEDULED_BOUND          128
#define SCORE_SCHEDULERCLUSTERLISTTOADDSTITCH_BOUND             64

#endif

