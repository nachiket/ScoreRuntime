/////////////////////////////////////////////////////////////////////////////
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
// $Revision: 1.3 $
//
//////////////////////////////////////////////////////////////////////////////

#include <iostream.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <values.h>
#include <dlfcn.h>
#include "LEDA/core/list.h"
#include "LEDA/graph/graph.h"
#include "LEDA/basic_graph_alg.h"
#include "LEDA/graph_alg.h"
#include "ScoreOperatorInstance.h"
#include "ScoreStream.h"
#include "ScoreStreamStitch.h"
#include "ScorePage.h"
#include "ScoreSegment.h"
#include "ScoreSegmentStitch.h"
#include "ScoreCluster.h"
#include "ScoreProcess.h"
#include "ScoreProcessorNode.h"
#include "ScoreArray.h"
#include "ScoreType.h"
#include "ScoreHardwareAPI.h"
#include "ScoreSchedulerRandom.h"
#include "ScoreRuntime.h"
#include "ScoreSimulator.h"
#include "ScoreConfig.h"
#include "ScoreStateGraph.h"
#include "ScoreDummyDonePage.h"
#include "ScoreCustomStack.h"
#include "ScoreCustomList.h"
#include "ScoreCustomQueue.h"
#include "ScoreCustomLinkedList.h"

#include "ScoreSchedulerRandomDefines.h"

///////////////////////////////////////////////////////////////////////////////
// ScoreSchedulerRandom::performPlacement:
//   Given scheduledPageList, scheduledMemSegList, removedPageList, and
//     removedMemSegList, doneNotRemovedPageList, doneNotRemovedMemSegList,
//     faultedMemSegList find the best placement for the pages.
//
// Parameters: none.
//
// Return value: none.
///////////////////////////////////////////////////////////////////////////////
void ScoreSchedulerRandom::performPlacement() {
  unsigned int i, j;

  if (EXTRA_DEBUG) {
    cerr << "PLACEMENT begin: number of unused CMBs = " << 
      SCORECUSTOMLINKEDLIST_LENGTH(unusedPhysicalCMBs) << endl;
  }

#if PROFILE_PERFORMPLACEMENT
  unsigned long long startTime, endTime;
#endif

#if DOPROFILING
  startClock = threadCounter->ScoreThreadSchCounterRead();
#endif

#if PROFILE_PERFORMPLACEMENT
  startTime = threadCounter->ScoreThreadSchCounterRead();
#endif

#if DOSCHEDULE_FOR_SCHEDTIME
  startClock = threadCounter->ScoreThreadSchCounterRead();
#endif

  //////////////////////////////////////////////////////////////////////////
  // - REMOVE THE PAGES/SEGMENTS THAT ARE SUPPOSED TO BE REMOVED.
  // - IF THE PAGE/SEGMENT IS NOT DONE, THEN MARK ITS SEGMENT BLOCK CACHED 
  //     (BUT DO NOT ACTUALLY DUMP THEM BACK TO MAIN MEMORY).
  // - IF THE PAGE/SEGMENT IS DONE, THEN ACTUALLY FREE ITS SEGMENT BLOCK
  //     (IF THE SEGMENT IS MARKED DUMPONDONE, THEN ARRANGE FOR ITS DATA TO
  //      BE DUMPED BACK TO MAIN MEMORY).
  // - IF THERE WERE ANY PAGES/SEGMENTS WHICH WERE DONE BUT NOT CURRENTLY
  //     RESIDENT, THEN FREE THEIR CACHED SEGMENT BLOCKS (IF ANY).
  //     (IF A SEGMENT IS MARKED DUMPONDONE, THEN ARRANGE FOR ITS DATA TO
  //      BE DUMPED BACK TO MAIN MEMORY).
  //////////////////////////////////////////////////////////////////////////

  // remove the pages/memory segments that were indicated.
  for (i = 0; i < SCORECUSTOMLIST_LENGTH(removedPageList); i++) {
    ScorePage *removedPage;
    unsigned int removedPageLoc;
    ScoreSegmentBlock *cachedBlock;
    ScoreSegmentTable *cachedTable;

    SCORECUSTOMLIST_ITEMAT(removedPageList, i, removedPage);
    removedPageLoc = removedPage->sched_residentLoc;
    cachedBlock = removedPage->sched_cachedSegmentBlock;
    cachedTable = cachedBlock->parentTable;

    arrayCP[removedPageLoc].scheduled = NULL;

    SCORECUSTOMQUEUE_QUEUE(unusedPhysicalCPs, removedPageLoc);

    if (removedPage->sched_isDone) {
      cachedTable->freeUsedLevel1Block(cachedBlock);
      removedPage->sched_cachedSegmentBlock = NULL;
    } else {
      cachedTable->markCachedLevel1Block(cachedBlock);

      removedPage->sched_dumpSegmentBlock = 
        removedPage->sched_cachedSegmentBlock;
    }
  }
  for (i = 0; i < SCORECUSTOMLIST_LENGTH(removedMemSegList); i++) {
    ScoreSegment *removedSegment;
    unsigned int removedSegmentLoc;
    ScoreSegmentBlock *cachedBlock;
    ScoreSegmentTable *cachedTable;

    SCORECUSTOMLIST_ITEMAT(removedMemSegList, i, removedSegment);
    removedSegmentLoc = removedSegment->sched_residentLoc;
    cachedBlock = removedSegment->sched_cachedSegmentBlock;
    cachedTable = cachedBlock->parentTable;

    arrayCMB[removedSegmentLoc].scheduled = NULL;
    
    SCORECUSTOMLINKEDLIST_APPEND(unusedPhysicalCMBs, 
				 removedSegmentLoc, 
				 arrayCMB[removedSegmentLoc].unusedPhysicalCMBsItem);

    if (removedSegment->sched_isDone) {
      if (removedSegment->sched_dumpOnDone) {
	if (!(removedSegment->sched_isStitch)) {
	  // FIX ME! SHOULD REALLY ADVANCE DATA BUFFER POINTER ACCORDING TO
	  // ACTUAL DATA WIDTH!
	  cachedTable->addToDumpBlockList(cachedBlock, 0,
	    (((unsigned long long *) removedSegment->data())+
	     removedSegment->sched_residentStart),
	    removedSegment->sched_residentLength*(SCORE_ALIGN_BITWIDTH_TO_8(removedSegment->width())/8));
	} else {
	  // FIX ME! SHOULD REALLY ADVANCE DATA BUFFER POINTER ACCORDING TO
	  // ACTUAL DATA WIDTH!
	  cachedTable->addToDumpBlockList_useOwnerAddrs(cachedBlock, 0,
	    (((unsigned long long *) removedSegment->data())+
	     removedSegment->sched_residentStart));
	}
      }

      cachedTable->freeUsedLevel0Block(cachedBlock);
      removedSegment->sched_cachedSegmentBlock = NULL;
    } else {
      cachedTable->markCachedLevel0Block(cachedBlock);

      removedSegment->sched_dumpSegmentBlock = 
        removedSegment->sched_cachedSegmentBlock;
    }
  }
  for (i = 0; i < SCORECUSTOMLIST_LENGTH(doneNotRemovedPageList); i++) {
    ScorePage *donePage;

    SCORECUSTOMLIST_ITEMAT(doneNotRemovedPageList, i, donePage);

    if (donePage->sched_cachedSegmentBlock != NULL) {
      ScoreSegmentBlock *cachedBlock = 
	donePage->sched_cachedSegmentBlock;
      ScoreSegmentTable *cachedTable = cachedBlock->parentTable;

      cachedTable->freeCachedLevel1Block(cachedBlock);
      donePage->sched_cachedSegmentBlock = NULL;
    }
  }
  SCORECUSTOMLIST_CLEAR(doneNotRemovedPageList);
  for (i = 0; i < SCORECUSTOMLIST_LENGTH(doneNotRemovedMemSegList); i++) {
    ScoreSegment *doneSegment;

    SCORECUSTOMLIST_ITEMAT(doneNotRemovedMemSegList, i, doneSegment);

    if (doneSegment->sched_cachedSegmentBlock != NULL) {
      ScoreSegmentBlock *cachedBlock = 
	doneSegment->sched_cachedSegmentBlock;
      ScoreSegmentTable *cachedTable = cachedBlock->parentTable;

      if (doneSegment->sched_dumpOnDone) {
	if (!(doneSegment->sched_isStitch)) {
	  // FIX ME! SHOULD REALLY ADVANCE DATA BUFFER POINTER ACCORDING TO
	  // ACTUAL DATA WIDTH!
	  cachedTable->addToDumpBlockList(cachedBlock, 0,
            (((unsigned long long *) doneSegment->data())+
             doneSegment->sched_residentStart),
            doneSegment->sched_residentLength*(SCORE_ALIGN_BITWIDTH_TO_8(doneSegment->width())/8));
	} else {
	  // FIX ME! SHOULD REALLY ADVANCE DATA BUFFER POINTER ACCORDING TO
	  // ACTUAL DATA WIDTH!
	  cachedTable->addToDumpBlockList_useOwnerAddrs(cachedBlock, 0,
            (((unsigned long long *) doneSegment->data())+
             doneSegment->sched_residentStart));
	}
      }

      cachedTable->freeCachedLevel0Block(cachedBlock);
      doneSegment->sched_cachedSegmentBlock = NULL;
    }
  }
  SCORECUSTOMLIST_CLEAR(doneNotRemovedMemSegList);

#if PROFILE_PERFORMPLACEMENT
  endTime = threadCounter->ScoreThreadSchCounterRead();
  cerr << "****** PERFORMPLACMENT0: " << endTime-startTime << endl;

  startTime = threadCounter->ScoreThreadSchCounterRead();
#endif

  //////////////////////////////////////////////////////////////////////////
  // - CHECK ALL OF THE PAGES WHICH ARE TO BE ADDED THAT HAVE THEIR 
  //     SEGMENT BLOCKS CURRENTLY CACHED ON THE ARRAY IN A CMB. MARK 
  //     THOSE CACHED SEGMENT BLOCKS AS USED TO KEEP THEM FROM BEING EVICTED.
  // - IN ADDITION, LOCK DOWN THE LOCATIONS FOR ALL OF THE ADDED PAGES
  //     INTO FREE PHYSICAL PAGES (CURRENTLY, NO PLACEMENT OPTIMIZATION
  //     IS CONSIDERED).
  // - CHECK ALL OF THE SEGMENTS WHICH ARE TO BE ADDED THAT HAVE THEIR
  //     SEGMENT BLOCKS CURRENTLY CACHED ON THE ARRAY IN A CMB. IF THE
  //     CMB IN WHICH THE SEGMENT BLOCK IS CACHED IS CURRENTLY FREE, THEN
  //     LOCK THE SEGMENT INTO THE FREE PHYSICAL CMB AS WELL AS MARKING
  //     THE CACHED SEGMENT BLOCK AS USED TO KEEP IT FROM BEING EVICTED.
  //     OTHERWISE, ARRANGE FOR THE CACHED SEGMENT BLOCK TO BE DUMPED
  //     BACK TO MAIN MEMORY (IF THE SEGMENT IS SEQ/RAMSRC THEN DO NOT
  //     DUMP BACK THE DATA).
  //////////////////////////////////////////////////////////////////////////

  // FIX ME! PERHAPS TRY TO AVOID ALLOCATING BLOCKS IN CMBS WHICH WILL BE
  //         USED FOR LOADING CONFIG/STATE/FIFO ON THIS TIMESLICE!

  // FIX ME! SHOULD ALLOW CACHING IN MULTIPLE LOCATIONS!

  // FIX ME! LATER ON, SHOULD TRY CACHING OF THE CONFIG/STATE/FIFO INSIDE OF
  //         CPS/CMBS THENSELF AND SKIP THE LOADING PROCESS ALSO!

  // FIX ME! SHOULD TRY TO DO CMB-TO-CMB DIRECT TRANSFERS INSTEAD OF
  //         DUMPING TO PRIMARY FIRST THEM BACK TO ARRAY!

  // FIX ME! SHOULD FAVOR EVICTING SEGMENTS THAT ARE READ-ONLY!

  // make sure that, if any of the scheduled pages/segments have cached
  //   segment blocks, those blocks are locked to prevent them from being
  //   freed.
  // try to lock down the locations for scheduled segments if they already
  //   have cached data segments.
  // we can first place all of the pages, because their placement currently
  // is relatively unimportant. (i.e. it will not affect allocation of
  // blocks in CMBs).
  // NOTE: FIX ME! In the future, this may not be the case when we start
  //       being more intelligent and worrying about placement/routing
  //       etc.
  for (i = 0; i < SCORECUSTOMLIST_LENGTH(scheduledPageList); i++) {
    ScorePage *addedPage;
    unsigned int unusedLoc;

    SCORECUSTOMLIST_ITEMAT(scheduledPageList, i, addedPage);
    SCORECUSTOMQUEUE_DEQUEUE(unusedPhysicalCPs, unusedLoc);

    arrayCP[unusedLoc].scheduled = addedPage;
    addedPage->sched_residentLoc = unusedLoc;

    if (addedPage->sched_cachedSegmentBlock != NULL) {
      ScoreSegmentBlock *cachedBlock = addedPage->sched_cachedSegmentBlock;
      ScoreSegmentTable *cachedTable = cachedBlock->parentTable;

      cachedTable->markUsedLevel1Block(cachedBlock);
    }
  }
  for (i = 0; i < SCORECUSTOMLIST_LENGTH(scheduledMemSegList); i++) {
    ScoreSegment *addedSegment;

    SCORECUSTOMLIST_ITEMAT(scheduledMemSegList, i, addedSegment);

    if (addedSegment->sched_cachedSegmentBlock != NULL) {
      ScoreSegmentBlock *cachedBlock = addedSegment->sched_cachedSegmentBlock;
      ScoreSegmentTable *cachedTable = cachedBlock->parentTable;
      unsigned int cachedLoc = cachedTable->loc;

      // check to see if the CMB location is free; if so, lock down the
      // data segment block and assign the segment to this CMB.
      if (arrayCMB[cachedLoc].scheduled == NULL) {
	arrayCMB[cachedLoc].scheduled = addedSegment;
	addedSegment->sched_residentLoc = cachedLoc;

	SCORECUSTOMLINKEDLIST_DELITEM(unusedPhysicalCMBs,
				      arrayCMB[cachedLoc].unusedPhysicalCMBsItem);

	cachedTable->markUsedLevel0Block(cachedBlock);

	addedSegment->sched_pboAddr = cachedBlock->start;
      } else {
	ScoreSegmentBlock *cachedBlock =
	  addedSegment->sched_cachedSegmentBlock;

	// FIX ME! IF WE WERE DOING CMB-TO-CMB TRANSFERS, WE SHOULDN'T DUMP
	//         THE SEGMENT BLOCK BACK TO MEMORY!

	// FIX ME! SHOULD REALLY ADVANCE DATA BUFFER POINTER ACCORDING TO
	// ACTUAL DATA WIDTH!
	if ((addedSegment->sched_mode != SCORE_CMB_SEQSRC) &&
	    (addedSegment->sched_mode != SCORE_CMB_RAMSRC)) {
	  if (!(addedSegment->sched_isStitch)) {
	    cachedTable->addToDumpBlockList(cachedBlock, (0<<2),
              (((unsigned long long *) addedSegment->data())+
	       addedSegment->sched_residentStart),
              addedSegment->sched_residentLength*(SCORE_ALIGN_BITWIDTH_TO_8(addedSegment->width())/8));
	  } else {
	    cachedTable->addToDumpBlockList_useOwnerAddrs(cachedBlock, (0<<2),
              (((unsigned long long *) addedSegment->data())+
	       addedSegment->sched_residentStart));
	  }
	}
	cachedTable->addToDumpBlockList(cachedBlock, 
	  SCORE_DATASEGMENTBLOCK_LOADSIZE,
          addedSegment->sched_fifoBuffer,
          SCORE_MEMSEGFIFO_SIZE);

	cachedTable->freeCachedLevel0Block(cachedBlock);
	addedSegment->sched_cachedSegmentBlock = NULL;
      }
    }
  }

  //////////////////////////////////////////////////////////////////////////
  // - FOR ALL REMAINING SEGMENTS THAT ARE TO BE ADDED BUT DO NOT HAVE
  //     THEIR DATA/FIFO CURRENTLY CACHED IN A FREE PHYSICAL CMB,
  //     ARBITRARILY ASSIGN IT TO A FREE PHYSICAL CMB. THEN ATTEMPT TO
  //     ALLOCATE A FREE LEVEL0 SEGMENT BLOCK IN THAT PHYSICAL CMB. IF
  //     NO FREE LEVEL0 SEGMENT BLOCK EXISTS IN THAT CMB, THEN EVICT A
  //     CACHED (BUT NOT USED) LEVEL0 SEGMENT BLOCK. (WE CAN GUARANTEE
  //     SUCCESS BECAUSE WE WILL GUARANTEE IN ALLOCATION THAT WE NEVER
  //     ALLOCATE ALL OF A CMB'S BLOCKS TO LEVEL1 BLOCKS).
  //     - WHEN EVICTING A LEVEL0 SEGMENT BLOCK, IF THE SEGMENT IS A
  //         SEQ/RAMSRC THEN ITS DATA WILL NOT BE DUMPED TO MAIN MEMORY.
  //     - WHEN LOADING A SEGMENT INTO A LEVEL0 SEGMENT BLOCK, IF THE
  //         FIFO DATA IS NOT VALID (I.E. THE SEGMENT HAS NEVER BEEN
  //         RESIDENT BEFORE), THE FIFO DATA IS NOT LOADED.
  // - FOR ALL REMAINING PAGES THAT ARE TO BE ADDED BUT DO NOT HAVE
  //     THEIR CONFIG/STATE/FIFO CURRENTLY CACHED IN A FREE PHYSICAL CMB,
  //     FIRST TRY TO FIND A FREE LEVEL1 SEGMENT BLOCK. IF NO SUCH SEGMENT
  //     BLOCK EXISTS, FIND A CACHED (BUT NOT USED) LEVEL1 SEGMENT BLOCK.
  //     IF NO SUCH SEGMENT BLOCK EXISTS, FIND A CACHED (BUT NOT USED)
  //     LEVEL0 SEGMENT BLOCK. WE ATTEMPT TO ALLOCATE THE SEGMENT BLOCKS
  //     EVENLY THROUGHOUT THE PHYSICAL CMBS TO MAXIMIZE PARALLEL LOAD/DUMP
  //     OPPORTUNITIES.
  //////////////////////////////////////////////////////////////////////////

  // since we are going to guarantee in the segment tables of each CMB
  // that there will at least be a level0 block for the resident segment
  // (we may have to bump off a cached segment, but we should never have to
  // bump off a level1 block for a currently resident page), then we
  // can simply assign the remaining segments to whatever unused physical CMB
  // there are left.
  for (i = 0; i < SCORECUSTOMLIST_LENGTH(scheduledMemSegList); i++) {
    ScoreSegment *addedSegment;
    ScoreSegmentStitch *addedStitch;

    SCORECUSTOMLIST_ITEMAT(scheduledMemSegList, i, addedSegment);
    addedStitch = (ScoreSegmentStitch *) addedSegment;

    if (addedSegment->sched_cachedSegmentBlock == NULL) {
      unsigned int unusedLoc;
      ScoreSegmentBlock *cachedBlock = NULL;
      ScoreSegmentTable *cachedTable;

      SCORECUSTOMLINKEDLIST_POP(unusedPhysicalCMBs, unusedLoc);
      cachedTable = arrayCMB[unusedLoc].segmentTable;

      arrayCMB[unusedLoc].scheduled = addedSegment;
      addedSegment->sched_residentLoc = unusedLoc;

      arrayCMB[unusedLoc].unusedPhysicalCMBsItem = SCORECUSTOMLINKEDLIST_NULL;

      cachedBlock = cachedTable->allocateLevel0Block(addedSegment);
    
      // if we could not get a free block, then we will have to forcibly
      // evict a cached block.
      if (cachedBlock == NULL) {
	// we will now forcibly evict a cached block.
	// FIX ME! IN THE FUTURE, WE MAY WANT TO CONSIDER EVICTING LEVEL1
	//         BLOCKS ALSO! FOR NOW, KEEP IT SIMPLE AND JUST EVICT A
	//         LEVEL0 BLOCK SINCE IT IS GUARANTEED THAT THERE IS AN
	//         UNUSED CACHED LEVEL0 BLOCK AVAILABLE!
	
	ScoreSegmentBlock *evictedBlock;
	ScoreGraphNode *evictedNode;
	ScoreSegment *evictedSegment;

	SCORECUSTOMLINKEDLIST_FRONT(cachedTable->cachedList_Level0Blocks,
				    evictedBlock);
	evictedNode = evictedBlock->owner;
	evictedSegment = (ScoreSegment *) evictedNode;

	// FIX ME! SHOULD REALLY ADVANCE DATA BUFFER POINTER ACCORDING TO
	// ACTUAL DATA WIDTH!
	if ((evictedSegment->sched_mode != SCORE_CMB_SEQSRC) &&
	    (evictedSegment->sched_mode != SCORE_CMB_RAMSRC)) {
	  if (!(evictedSegment->sched_isStitch)) {
	    cachedTable->addToDumpBlockList(evictedBlock, 0,
              (((unsigned long long *) evictedSegment->data())+
	       evictedSegment->sched_residentStart),
              evictedSegment->sched_residentLength*(SCORE_ALIGN_BITWIDTH_TO_8(evictedSegment->width())/8));
	  } else {
	    cachedTable->addToDumpBlockList_useOwnerAddrs(evictedBlock, 0,
              (((unsigned long long *) evictedSegment->data())+
	       evictedSegment->sched_residentStart));
	  }
	}
	cachedTable->addToDumpBlockList(evictedBlock, 
          SCORE_DATASEGMENTBLOCK_LOADSIZE,
          evictedSegment->sched_fifoBuffer,
          SCORE_MEMSEGFIFO_SIZE);

	cachedTable->freeCachedLevel0Block(evictedBlock);
	evictedSegment->sched_cachedSegmentBlock = NULL;

	cachedBlock = cachedTable->allocateLevel0Block(addedSegment);
      }

      addedSegment->sched_cachedSegmentBlock = cachedBlock;
      
      addedSegment->sched_pboAddr = cachedBlock->start;

      if (!(addedSegment->sched_isStitch && addedStitch->sched_isNewStitch)) {
	if (!(addedSegment->sched_isStitch)) {
	  cachedTable->addToLoadBlockList(cachedBlock, 0,
            (((unsigned long long *) addedSegment->data())+
             addedSegment->sched_residentStart),
            addedSegment->sched_residentLength*(SCORE_ALIGN_BITWIDTH_TO_8(addedSegment->width())/8));
	} else {
	  cachedTable->addToLoadBlockList_useOwnerAddrs(cachedBlock, 0,
            (((unsigned long long *) addedSegment->data())+
             addedSegment->sched_residentStart));
	}
      } else {
        addedStitch->sched_isNewStitch = 0;
      }
      if (addedSegment->sched_isFIFOBufferValid) {
	cachedTable->addToLoadBlockList(cachedBlock,
          SCORE_DATASEGMENTBLOCK_LOADSIZE,
          addedSegment->sched_fifoBuffer,
          SCORE_MEMSEGFIFO_SIZE);
      }
    }
  }

  // for now, since the location of the config/state/fifo for a page are
  // unimportant (except for the fact that we would like to spread them
  // out to increase parallel context switching opportunities) then we
  // simply need to find any free level1 block (or create such a block
  // by evicting a cached level0 or level1 block).
  for (i = 0; i < SCORECUSTOMLIST_LENGTH(scheduledPageList); i++) {
    ScorePage *addedPage;

    SCORECUSTOMLIST_ITEMAT(scheduledPageList, i, addedPage);

    if (addedPage->sched_cachedSegmentBlock == NULL) {
      unsigned int cachedTableLoc = 0;
      ScoreSegmentBlock *cachedBlock = NULL;
      ScoreSegmentTable *cachedTable = NULL;

      // randomly pick a CMB to try. then, try to get a free level1, then
      // try to evict a level1, finally try to evict a level0... do this
      // numPhysicalCMB number of times... if this still fails, then
      // we will try 1 last ditch effort to sequentially go through the
      // CMBs... if that finally fails, then we don't have enough resources
      // to cache the config/state/fifo for all resident CPs.
      for (j = 0; j < numPhysicalCMB; j++) {
	cachedTableLoc = random()%numPhysicalCMB;
	cachedTable = arrayCMB[cachedTableLoc].segmentTable;
	cachedBlock = cachedTable->allocateLevel1Block(addedPage);

	if (cachedBlock != NULL) {
	  break;
	}
	
	if (!(SCORECUSTOMLINKEDLIST_ISEMPTY(cachedTable->cachedList_Level1Blocks))) {
	  ScoreSegmentBlock *evictedBlock;
	  ScoreGraphNode *evictedNode;
	  ScorePage *evictedPage;

	  SCORECUSTOMLINKEDLIST_FRONT(cachedTable->cachedList_Level1Blocks,
				      evictedBlock);
	  evictedNode = evictedBlock->owner;
	  evictedPage = (ScorePage *) evictedNode;
	      
	  cachedTable->addToDumpBlockList(evictedBlock,
	    SCORE_PAGECONFIG_SIZE, 
            evictedPage->bitstream(),
	    SCORE_PAGESTATE_SIZE);
	  cachedTable->addToDumpBlockList(evictedBlock,
            (SCORE_PAGECONFIG_SIZE+SCORE_PAGESTATE_SIZE),
            evictedPage->sched_fifoBuffer,
            SCORE_PAGEFIFO_SIZE);

	  cachedTable->freeCachedLevel1Block_nomerge(evictedBlock);
	  evictedPage->sched_cachedSegmentBlock = NULL;
	      
	  cachedBlock = cachedTable->allocateLevel1Block(addedPage);
	      
	  break;
	}

	// NOTE: We will skip any CMB which does not have more than
	//       1 level0 block combined in cached and free list!
	//       This is to guarantee that segments resident in a CMB
	//       will always be able to get a level0 segment block
	//       without having to evict a used level1 segment block!
	if (!(SCORECUSTOMLINKEDLIST_ISEMPTY(cachedTable->cachedList_Level0Blocks)) &&
	    ((SCORECUSTOMLINKEDLIST_LENGTH(cachedTable->cachedList_Level0Blocks)+
	      SCORECUSTOMLINKEDLIST_LENGTH(cachedTable->freeList_Level0Blocks)+
	      SCORECUSTOMLINKEDLIST_LENGTH(cachedTable->usedList_Level0Blocks)) != 1)) {
	  ScoreSegmentBlock *evictedBlock;
	  ScoreGraphNode *evictedNode;
	  ScoreSegment *evictedSegment;

	  SCORECUSTOMLINKEDLIST_FRONT(cachedTable->cachedList_Level0Blocks,
				      evictedBlock);
	  evictedNode = evictedBlock->owner;
	  evictedSegment = (ScoreSegment *) evictedNode;

	  // FIX ME! SHOULD REALLY ADVANCE DATA BUFFER POINTER ACCORDING TO
	  // ACTUAL DATA WIDTH!
	  if ((evictedSegment->sched_mode != SCORE_CMB_SEQSRC) &&
	      (evictedSegment->sched_mode != SCORE_CMB_RAMSRC)) {
	    if (!(evictedSegment->sched_isStitch)) {
	      cachedTable->addToDumpBlockList(evictedBlock, 0,
                (((unsigned long long *) evictedSegment->data())+
	         evictedSegment->sched_residentStart),
                evictedSegment->sched_residentLength*(SCORE_ALIGN_BITWIDTH_TO_8(evictedSegment->width())/8));
	    } else {
	      cachedTable->addToDumpBlockList_useOwnerAddrs(evictedBlock, 0,
                (((unsigned long long *) evictedSegment->data())+
	         evictedSegment->sched_residentStart));
	    }
	  }
	  cachedTable->addToDumpBlockList(evictedBlock, 
            SCORE_DATASEGMENTBLOCK_LOADSIZE,
            evictedSegment->sched_fifoBuffer,
            SCORE_MEMSEGFIFO_SIZE);

	  cachedTable->freeCachedLevel0Block(evictedBlock);
	  evictedSegment->sched_cachedSegmentBlock = NULL;

	  cachedBlock = cachedTable->allocateLevel1Block(addedPage);
	      
	  break;
	}
      }
      if (cachedBlock == NULL) {
	for (cachedTableLoc = 0; cachedTableLoc < numPhysicalCMB; cachedTableLoc++) {
	  cachedTable = arrayCMB[cachedTableLoc].segmentTable;
	  cachedBlock = cachedTable->allocateLevel1Block(addedPage);

	  if (cachedBlock != NULL) {
	    break;
	  }
	
	  if (!(SCORECUSTOMLINKEDLIST_ISEMPTY(cachedTable->cachedList_Level1Blocks))) {
	    ScoreSegmentBlock *evictedBlock;
	    ScoreGraphNode *evictedNode;
	    ScorePage *evictedPage;
	      
	    SCORECUSTOMLINKEDLIST_FRONT(cachedTable->cachedList_Level1Blocks,
					evictedBlock);
	    evictedNode = evictedBlock->owner;
	    evictedPage = (ScorePage *) evictedNode;
	      
	    cachedTable->addToDumpBlockList(evictedBlock,
					    SCORE_PAGECONFIG_SIZE, 
					    evictedPage->bitstream(),
					    SCORE_PAGESTATE_SIZE);
	    cachedTable->addToDumpBlockList(evictedBlock,
					    (SCORE_PAGECONFIG_SIZE+SCORE_PAGESTATE_SIZE),
					    evictedPage->sched_fifoBuffer,
					    SCORE_PAGEFIFO_SIZE);

	    cachedTable->freeCachedLevel1Block_nomerge(evictedBlock);
	    evictedPage->sched_cachedSegmentBlock = NULL;
	      
	    cachedBlock = cachedTable->allocateLevel1Block(addedPage);
	      
	    break;
	  }

	  // NOTE: We will skip any CMB which does not have more than
	  //       1 level0 block combined in cached and free list!
	  //       This is to guarantee that segments resident in a CMB
	  //       will always be able to get a level0 segment block
	  //       without having to evict a used level1 segment block!
	  if (!(SCORECUSTOMLINKEDLIST_ISEMPTY(cachedTable->cachedList_Level0Blocks)) &&
	      ((SCORECUSTOMLINKEDLIST_LENGTH(cachedTable->cachedList_Level0Blocks)+
		SCORECUSTOMLINKEDLIST_LENGTH(cachedTable->freeList_Level0Blocks)+
		SCORECUSTOMLINKEDLIST_LENGTH(cachedTable->usedList_Level0Blocks)) != 1)) {
	    ScoreSegmentBlock *evictedBlock;
	    ScoreGraphNode *evictedNode;
	    ScoreSegment *evictedSegment;

	    SCORECUSTOMLINKEDLIST_FRONT(cachedTable->cachedList_Level0Blocks,
					evictedBlock);
	    evictedNode = evictedBlock->owner;
	    evictedSegment = (ScoreSegment *) evictedNode;

	    // FIX ME! SHOULD REALLY ADVANCE DATA BUFFER POINTER ACCORDING TO
	    // ACTUAL DATA WIDTH!
	    if ((evictedSegment->sched_mode != SCORE_CMB_SEQSRC) &&
		(evictedSegment->sched_mode != SCORE_CMB_RAMSRC)) {
	      if (!(evictedSegment->sched_isStitch)) {
		cachedTable->addToDumpBlockList(evictedBlock, 0,
	          (((unsigned long long *) evictedSegment->data())+
	          evictedSegment->sched_residentStart),
	          evictedSegment->sched_residentLength*(SCORE_ALIGN_BITWIDTH_TO_8(evictedSegment->width())/8));
	      } else {
		cachedTable->addToDumpBlockList_useOwnerAddrs(evictedBlock, 0,
	          (((unsigned long long *) evictedSegment->data())+
	          evictedSegment->sched_residentStart));
	      }
	    }
	    cachedTable->addToDumpBlockList(evictedBlock, 
					    SCORE_DATASEGMENTBLOCK_LOADSIZE,
					    evictedSegment->sched_fifoBuffer,
					    SCORE_MEMSEGFIFO_SIZE);

	    cachedTable->freeCachedLevel0Block(evictedBlock);
	    evictedSegment->sched_cachedSegmentBlock = NULL;

	    cachedBlock = cachedTable->allocateLevel1Block(addedPage);
	      
	    break;
	  }
	}
      }

      // FIX ME! THERE MIGHT BE A SITUATION WHERE THERE JUST ISN'T ENOUGH
      // CMBS TO CACHE ALL CPS!
      if (cachedBlock == NULL) {
	cerr << "FIX ME! NOT ENOUGH PHYSICAL CMBS TO CACHE ALL RESIDENT CPS!"
	     << endl;
	exit(1);
      }

      addedPage->sched_cachedSegmentBlock = cachedBlock;

      cachedTable->addToLoadBlockList(cachedBlock, 0,
        addedPage->bitstream(),
        SCORE_PAGECONFIG_SIZE);
      cachedTable->addToLoadBlockList(cachedBlock, 
        SCORE_PAGECONFIG_SIZE,
        addedPage->bitstream(),
        SCORE_PAGESTATE_SIZE);
      if (addedPage->sched_isFIFOBufferValid) {
	cachedTable->addToLoadBlockList(cachedBlock,
          (SCORE_PAGECONFIG_SIZE+SCORE_PAGESTATE_SIZE),
          addedPage->sched_fifoBuffer,
          SCORE_PAGEFIFO_SIZE);
      }
    }
  }

#if PROFILE_PERFORMPLACEMENT
  endTime = threadCounter->ScoreThreadSchCounterRead();
  cerr << "****** PERFORMPLACMENT1: " << endTime-startTime << endl;

  startTime = threadCounter->ScoreThreadSchCounterRead();
#endif

  //////////////////////////////////////////////////////////////////////////
  // - FOR ALL FAULTED MEMORY SEGMENTS, LOAD THE NEW SECTION OF DATA INTO
  //     THE SEGMENT BLOCK. IF THE SEGMENT IS SEQ/RAM*SINK THEN FIRST
  //     ARRANGE FOR THE CURRENT CONTENTS TO BE DUMPED BACK TO MAIN MEMORY.
  //////////////////////////////////////////////////////////////////////////

  for (i = 0; i < SCORECUSTOMLIST_LENGTH(faultedMemSegList); i++) {
    ScoreSegment *faultedSegment;
    ScoreSegmentBlock *cachedBlock;
    ScoreSegmentTable *cachedTable;
    unsigned int newBlockStart, newBlockEnd, newBlockLength;
    unsigned int oldTRA;

    SCORECUSTOMLIST_ITEMAT(faultedMemSegList, i, faultedSegment);
    cachedBlock = faultedSegment->sched_cachedSegmentBlock;
    cachedTable = cachedBlock->parentTable;
    oldTRA = faultedSegment->sched_traAddr;

    // FIX ME! SHOULD REALLY ADVANCE DATA BUFFER POINTER ACCORDING TO
    // ACTUAL DATA WIDTH!
    if ((faultedSegment->sched_mode != SCORE_CMB_SEQSRC) &&
	(faultedSegment->sched_mode != SCORE_CMB_RAMSRC)) {
      cachedTable->addToDumpBlockList(cachedBlock, 0,
        (((unsigned long long *) faultedSegment->data())+
	 faultedSegment->sched_residentStart),
	faultedSegment->sched_residentLength*(SCORE_ALIGN_BITWIDTH_TO_8(faultedSegment->width())/8));
    }

    // figure out what the new TRA and block size will be.
    newBlockStart =
      (faultedSegment->sched_faultedAddr/
       (SCORE_DATASEGMENTBLOCK_LOADSIZE/(SCORE_ALIGN_BITWIDTH_TO_8(faultedSegment->width())/8)))*
      (SCORE_DATASEGMENTBLOCK_LOADSIZE/(SCORE_ALIGN_BITWIDTH_TO_8(faultedSegment->width())/8));
    newBlockEnd = newBlockStart+
      (SCORE_DATASEGMENTBLOCK_LOADSIZE/(SCORE_ALIGN_BITWIDTH_TO_8(faultedSegment->width())/8));
    if (newBlockEnd > ((unsigned int) faultedSegment->length())) {
      newBlockEnd = faultedSegment->length();
    }
    newBlockLength = newBlockEnd-newBlockStart;
	  
    // store the new TRA and MAX.
    faultedSegment->sched_traAddr = newBlockStart;
    faultedSegment->sched_maxAddr = newBlockLength;
    
    faultedSegment->sched_residentStart = newBlockStart;
    faultedSegment->sched_residentLength = newBlockLength;

    // it is possible that the segment has not been rescheduled
    if (faultedSegment->getLastTimesliceScheduled() == currentTimeslice) {
      
      // FIX ME! SHOULD REALLY ADVANCE DATA BUFFER POINTER ACCORDING TO
      // ACTUAL DATA WIDTH!
      cachedTable->addToLoadBlockList(cachedBlock, 0,
				      (((unsigned long long *) faultedSegment->data())+
				       faultedSegment->sched_residentStart),
				      faultedSegment->sched_residentLength*(SCORE_ALIGN_BITWIDTH_TO_8(faultedSegment->width())/8));
      
    } else {
      assert (faultedSegment->getLastTimesliceScheduled() == currentTimeslice - 1);
      unsigned int faultedSegmentLoc = faultedSegment->sched_residentLoc;
      SCORECUSTOMLINKEDLIST_APPEND(unusedPhysicalCMBs, 
				   faultedSegmentLoc, 
				   arrayCMB[faultedSegmentLoc].unusedPhysicalCMBsItem);

      arrayCMB[faultedSegmentLoc].scheduled = NULL;

      cachedTable->addToDumpBlockList(cachedBlock, 
				      SCORE_DATASEGMENTBLOCK_LOADSIZE,
				      faultedSegment->sched_fifoBuffer,
				      SCORE_MEMSEGFIFO_SIZE);
      cachedTable->freeUsedLevel0Block(cachedBlock);
      faultedSegment->sched_cachedSegmentBlock = 0;
    }
    
    faultedSegment->sched_isFaulted = 0;

    if (VERBOSEDEBUG || DEBUG) {
      cerr << "SCHED: Changing TRA from " << oldTRA <<
	" to " << faultedSegment->sched_traAddr << endl;
    }
  }

#if DOSCHEDULE_FOR_SCHEDTIME
  endClock = threadCounter->ScoreThreadSchCounterRead();
  diffClock = endClock - startClock;
  current_performPlacement = diffClock;
#endif

#if PROFILE_PERFORMPLACEMENT
  endTime = threadCounter->ScoreThreadSchCounterRead();
  cerr << "****** PERFORMPLACMENT0: " << endTime-startTime << endl;
#endif

#if DOPROFILING
  endClock = threadCounter->ScoreThreadSchCounterRead();
  diffClock = endClock - startClock;
  if (diffClock < min_performPlacement) {
    min_performPlacement = diffClock;
  }
  if (diffClock > max_performPlacement) {
    max_performPlacement = diffClock;
  }
  total_performPlacement = total_performPlacement + diffClock;
  current_performPlacement = diffClock;
  cerr << "   performPlacement() ==> " << 
    diffClock <<
    " cycle(s)" << endl;
#endif

  if (EXTRA_DEBUG) {
    cerr << "PLACEMENT end: number of unused CMBs = " << 
      SCORECUSTOMLINKEDLIST_LENGTH(unusedPhysicalCMBs) << endl;
  }

}


///////////////////////////////////////////////////////////////////////////////
// ScoreSchedulerRandom::issueReconfigCommands:
//   Issue the reconfiguration commands to the array in order to load/dump
//     the correct pages/memory segments.
//
// Parameters: none.
//
// Return value: none.
///////////////////////////////////////////////////////////////////////////////
void ScoreSchedulerRandom::issueReconfigCommands() {
  unsigned int i;
  list<ScorePage *> dumpPageState_todo;
  list<ScorePage *> dumpPageFIFO_todo;
  list<ScoreSegment *> dumpSegmentFIFO_todo;
  list<ScorePage *> loadPageConfig_todo;
  list<ScorePage *> loadPageState_todo;
  list<ScorePage *> loadPageFIFO_todo;
  list<ScoreSegment *> loadSegmentFIFO_todo;


#if 0 && DOPROFILING
  startClock = threadCounter->ScoreThreadSchCounterRead();
#endif

  // if there is nothing to do, then just return.
  if ((SCORECUSTOMLIST_LENGTH(scheduledPageList) == 0) &&
      (SCORECUSTOMLIST_LENGTH(scheduledMemSegList) == 0) &&
      (SCORECUSTOMLIST_LENGTH(removedPageList) == 0) &&
      (SCORECUSTOMLIST_LENGTH(removedMemSegList) == 0) &&
      (SCORECUSTOMLIST_LENGTH(configChangedStitchSegList) == 0) &&
      (SCORECUSTOMLIST_LENGTH(faultedMemSegList) == 0)) {
    return;
  }

  // stop every page and CMB.
  batchCommandBegin();
  for (i = 0; i < numPhysicalCP; i++) {
    if (arrayCP[i].active != NULL) {
      stopPage(arrayCP[i].active);
#if KEEPRECONFIGSTATISTICS
      total_stopPage++;
#endif
    }
  }
  for (i = 0; i < numPhysicalCMB; i++) {
    if (arrayCMB[i].active != NULL) {
      stopSegment(arrayCMB[i].active);
#if KEEPRECONFIGSTATISTICS
      total_stopSegment++;
#endif
    }
  }
  batchCommandEnd();

#if GET_FEEDBACK
  if (gFeedbackMode == MAKEFEEDBACK)
    makeFeedback();
#endif

  // Don't know how expensive this really would be. We may be able to
  // do a more limited status check. This is the only way to get accurate
  // stats. We don't really need all of this info!
  // Currently, just used to determine if a stitch buffer is actually
  // empty with no input FIFO data. 
  // FIX ME!
  getCurrentStatus();

  // if segments are being removed, then get the values of their pointers.
  // NOTE: The reason this has been moved so far up is that this is also
  //       where we determine if a stitch buffer is empty and can be removed.
  //       If it can, then we can avoid some array commands.
  for (i = 0; i < numPhysicalCMB; i++) {
    if ((arrayCMB[i].active != arrayCMB[i].scheduled) &&
	(arrayCMB[i].active != NULL)) {
      if (!(arrayCMB[i].active->sched_isDone) ||
	  arrayCMB[i].active->sched_dumpOnDone) {
	batchCommandBegin();
	getSegmentPointers(arrayCMB[i].active);
#if KEEPRECONFIGSTATISTICS
	total_getSegmentPointers++;
#endif
	batchCommandEnd();
      }

      if (!(arrayCMB[i].active->sched_isDone)) {
	if (arrayCMB[i].active->sched_isStitch) {
	  ScoreSegmentStitch *currentStitch = 
	    (ScoreSegmentStitch *) arrayCMB[i].active;

	  if (!(currentStitch->sched_mustBeInDataFlow)) {
	    if (cmbStatus[i].readAddr == cmbStatus[i].writeAddr) {
              // we need to make sure that there are no input FIFO tokens
              // and that this stitch buffer did not signal done but not
              // get caught before.
              if (!(cmbStatus[i].isDone) && 
                  (cmbStatus[i].inputFIFONumTokens[
                     SCORE_CMB_STITCH_DATAW_INNUM] == 0)) {
	        SCORECUSTOMLIST_APPEND(emptyStitchList, currentStitch);
	        currentStitch->sched_isEmptyAndWillBeRemoved = 1;
              }
	    }
	  }
	}
      }
    }
  }

  // FIX ME! DO THIS BETTER WHEN WE HAVE DETERMINED THIS VERSION WORKS.
  // WE ESSENTIALLY JUST STOP ALL PAGES, DUMP ALL OLD PAGES, LOAD IN NEW
  // PAGES, AND RUN.

  // FIX ME! SHOULD MAKE SURE THAT ANY INPUTS/OUTPUTS THAT ARE NOT CONNECTED
  // ANYWHERE ARE PROPERLY TERMINATED (i.e. always assert empty or never
  // assert full!).

  // fill in the todo lists.
  for (i = 0; i < SCORECUSTOMLIST_LENGTH(scheduledPageList); i++) {
    ScorePage *currentPage;

    SCORECUSTOMLIST_ITEMAT(scheduledPageList, i, currentPage);

    loadPageConfig_todo.append(currentPage);
    loadPageState_todo.append(currentPage);
    loadPageFIFO_todo.append(currentPage);
  }
  loadPageConfig_todo.append(NULL);
  loadPageState_todo.append(NULL);
  loadPageFIFO_todo.append(NULL);

  for (i = 0; i < SCORECUSTOMLIST_LENGTH(scheduledMemSegList); i++) {
    ScoreSegment *currentSegment;

    SCORECUSTOMLIST_ITEMAT(scheduledMemSegList, i, currentSegment);

    loadSegmentFIFO_todo.append(currentSegment);
  }
  loadSegmentFIFO_todo.append(NULL);

  for (i = 0; i < SCORECUSTOMLIST_LENGTH(removedPageList); i++) {
    ScorePage *currentPage;

    SCORECUSTOMLIST_ITEMAT(removedPageList, i, currentPage);

    if (!(currentPage->sched_isDone)) {
      dumpPageState_todo.append(currentPage);
      dumpPageFIFO_todo.append(currentPage);
    }
  }
  dumpPageState_todo.append(NULL);
  dumpPageFIFO_todo.append(NULL);

  for (i = 0; i < SCORECUSTOMLIST_LENGTH(removedMemSegList); i++) {
    ScoreSegment *currentSegment;

    SCORECUSTOMLIST_ITEMAT(removedMemSegList, i, currentSegment);

    if (!(currentSegment->sched_isDone)) {
      if (!(currentSegment->sched_isStitch &&
	    ((ScoreSegmentStitch *) 
	     currentSegment)->sched_isEmptyAndWillBeRemoved)) {
	dumpSegmentFIFO_todo.append(currentSegment);
      }
    }
  }
  dumpSegmentFIFO_todo.append(NULL);

  // NOTE: WE TRY TO PARALLEL DUMP AS MANY OF THEM AS POSSIBLE FROM CMBs!
  while ((dumpPageState_todo.length() != 1) ||
	 (dumpPageFIFO_todo.length() != 1) ||
	 (dumpSegmentFIFO_todo.length() != 1)) {
    list<unsigned int> busyCPs, busyCMBs;
    ScorePage *currentPage = NULL;
    ScoreSegment *currentSegment = NULL;

    batchCommandBegin();

    currentPage = dumpPageState_todo.pop();
    while (currentPage != NULL) {
      unsigned int currentLoc = currentPage->sched_residentLoc;
      unsigned int cachedLoc = 
	currentPage->sched_dumpSegmentBlock->parentTable->loc;

      if (!(arrayCPBusy[currentLoc]) &&
	  !(arrayCMBBusy[cachedLoc])) {
	arrayCPBusy[currentLoc] = 1;
	arrayCMBBusy[cachedLoc] = 1;
	busyCPs.append(currentLoc);
	busyCMBs.append(cachedLoc);
	    
	dumpPageState(currentPage);
#if KEEPRECONFIGSTATISTICS
	total_dumpPageState++;
#endif
      } else {
	dumpPageState_todo.append(currentPage);
      }
      currentPage = dumpPageState_todo.pop();
    }
    dumpPageState_todo.append(NULL);

    currentPage = dumpPageFIFO_todo.pop();
    while (currentPage != NULL) {
      unsigned int currentLoc = currentPage->sched_residentLoc;
      unsigned int cachedLoc = 
	currentPage->sched_dumpSegmentBlock->parentTable->loc;
	    
      if (!(arrayCPBusy[currentLoc]) &&
	  !(arrayCMBBusy[cachedLoc])) {
	arrayCPBusy[currentLoc] = 1;
	arrayCMBBusy[cachedLoc] = 1;
	busyCPs.append(currentLoc);
	busyCMBs.append(cachedLoc);
	      
	dumpPageFIFO(currentPage);
#if KEEPRECONFIGSTATISTICS
	total_dumpPageFIFO++;
#endif
      } else {
	dumpPageFIFO_todo.append(currentPage);
      }
      currentPage = dumpPageFIFO_todo.pop();
    }
    dumpPageFIFO_todo.append(NULL);

    currentSegment = dumpSegmentFIFO_todo.pop();
    while (currentSegment != NULL) {
      unsigned int currentLoc = currentSegment->sched_residentLoc;
      unsigned int cachedLoc = 
	currentSegment->sched_dumpSegmentBlock->parentTable->loc;

      if (!(arrayCMBBusy[currentLoc]) &&
	  !(arrayCMBBusy[cachedLoc])) {
	arrayCMBBusy[currentLoc] = 1;
	arrayCMBBusy[cachedLoc] = 1;
	busyCMBs.append(currentLoc);
	busyCMBs.append(cachedLoc);
	    
	dumpSegmentFIFO(currentSegment);
#if KEEPRECONFIGSTATISTICS
	total_dumpSegmentFIFO++;
#endif
      } else {
	dumpSegmentFIFO_todo.append(currentSegment);
      }
      currentSegment = dumpSegmentFIFO_todo.pop();
    }
    dumpSegmentFIFO_todo.append(NULL);

    batchCommandEnd();

    // clear the busy flags.
    {
      unsigned int busyLoc;

      while (busyCPs.length() != 0) {
	busyLoc = busyCPs.pop();

	arrayCPBusy[busyLoc] = 0;
      }
      while (busyCMBs.length() != 0) {
	busyLoc = busyCMBs.pop();

	arrayCMBBusy[busyLoc] = 0;
      }
    }
  }

  // clear the CPs and CMBs that are changing.
  for (i = 0; i < numPhysicalCP; i++) {
    if ((arrayCP[i].active != arrayCP[i].scheduled) &&
	(arrayCP[i].active != NULL)) {
      arrayCP[i].active->sched_isResident = 0;
      arrayCP[i].active->sched_residentLoc = 0;
      arrayCP[i].active = NULL;
    }
  }
  for (i = 0; i < numPhysicalCMB; i++) {
    if ((arrayCMB[i].active != arrayCMB[i].scheduled) &&
	(arrayCMB[i].active != NULL)) {
      arrayCMB[i].active->sched_isResident = 0;
      arrayCMB[i].active->sched_residentLoc = 0;
      arrayCMB[i].active = NULL;
    }
  }

  // dump/load any blocks that need to be dumped/load back to/from 
  // primary memory.
  for (i = 0; i < numPhysicalCMB; i++) {
    unsigned int j;
    ScoreSegmentLoadDumpBlock *dumpBlocks =
      arrayCMB[i].segmentTable->dumpBlocks;
    unsigned int dumpBlocks_count =
      arrayCMB[i].segmentTable->dumpBlocks_count;
    ScoreSegmentLoadDumpBlock *loadBlocks =
      arrayCMB[i].segmentTable->loadBlocks;
    unsigned int loadBlocks_count =
      arrayCMB[i].segmentTable->loadBlocks_count;

    for (j = 0; j < dumpBlocks_count; j++) {
      ScoreSegmentLoadDumpBlock *dumpBlock = &(dumpBlocks[j]);

      if (!(dumpBlock->owner->isSegment() &&
	    ((ScoreSegment *) dumpBlock->owner)->sched_isStitch &&
	    ((ScoreSegmentStitch *) 
	     dumpBlock->owner)->sched_isEmptyAndWillBeRemoved)) {
	if (!(dumpBlock->bufferSizeDependsOnOwnerAddrs)) {
	  batchCommandBegin();
	  memXferCMBToPrimary(i,
			      dumpBlock->blockStart,
			      dumpBlock->bufferSize,
			      dumpBlock->buffer);
#if KEEPRECONFIGSTATISTICS
	  total_memXferCMBToPrimary++;
#endif
	  batchCommandEnd();
	} else {
	  // CURRENTLY THE ONLY SEGMENTS THAT HAVE THEIR DATA DUMPED/LOADED
	  // VIA bufferSizeDependsOnOwnerAddrs ARE STITCH BUFFERS!
	  ScoreSegmentStitch *dumpStitch = 
	    (ScoreSegmentStitch *) dumpBlock->owner;
	  unsigned int dumpStitchReadAddr = dumpStitch->sched_readAddr;
	  unsigned int dumpStitchWriteAddr = dumpStitch->sched_writeAddr;

#if VERBOSEDEBUG || DEBUG
	  unsigned int originalSize =
	    dumpStitch->length() * 
            (SCORE_ALIGN_BITWIDTH_TO_8(dumpStitch->width())/8);
	  unsigned int newSize = 0;
#endif

	  // ignore if stitch buffer is empty!
	  if (dumpStitchReadAddr != dumpStitchWriteAddr) {
	    if (dumpStitchReadAddr < dumpStitchWriteAddr) {
	      // the valid block is 1 contiguous section!
	      unsigned int dumpStitchSize = 
		(dumpStitchWriteAddr - dumpStitchReadAddr)*
		(SCORE_ALIGN_BITWIDTH_TO_8(dumpStitch->width())/8);
	      unsigned int dumpStitchStart =
		dumpBlock->blockStart +
		(dumpStitchReadAddr*(SCORE_ALIGN_BITWIDTH_TO_8(dumpStitch->width())/8));
	      void *dumpStitchBuffer =
		(void *) (((unsigned long long *) dumpBlock->buffer) +
			  dumpStitchReadAddr);

	      batchCommandBegin();
	      memXferCMBToPrimary(i,
				  dumpStitchStart,
				  dumpStitchSize,
				  dumpStitchBuffer);
#if KEEPRECONFIGSTATISTICS
	      total_memXferCMBToPrimary++;
#endif
	      batchCommandEnd();
#if VERBOSEDEBUG
	      newSize = newSize + dumpStitchSize;
#endif
	    } else {
	      // the valid block is actually 2 contiguous sections!
	      unsigned int dumpStitchSizeLower = 
		(dumpStitchWriteAddr - 0)*
		(SCORE_ALIGN_BITWIDTH_TO_8(dumpStitch->width())/8);
	      unsigned int dumpStitchStartLower =
		dumpBlock->blockStart +
		(0*(SCORE_ALIGN_BITWIDTH_TO_8(dumpStitch->width())/8));
	      void *dumpStitchBufferLower =
		(void *) (((unsigned long long *) dumpBlock->buffer) +
			  0);
	      unsigned int dumpStitchSizeUpper = 
		(dumpStitch->length() - dumpStitchReadAddr)*
		(SCORE_ALIGN_BITWIDTH_TO_8(dumpStitch->width())/8);
	      unsigned int dumpStitchStartUpper =
		dumpBlock->blockStart +
		(dumpStitchReadAddr*(SCORE_ALIGN_BITWIDTH_TO_8(dumpStitch->width())/8));
	      void *dumpStitchBufferUpper =
		(void *) (((unsigned long long *) dumpBlock->buffer) +
			  dumpStitchReadAddr);

	      batchCommandBegin();
	      memXferCMBToPrimary(i,
				  dumpStitchStartLower,
				  dumpStitchSizeLower,
				  dumpStitchBufferLower);
#if KEEPRECONFIGSTATISTICS
	      total_memXferCMBToPrimary++;
#endif
	      batchCommandEnd();

	      batchCommandBegin();
	      memXferCMBToPrimary(i,
				  dumpStitchStartUpper,
				  dumpStitchSizeUpper,
				  dumpStitchBufferUpper);
#if KEEPRECONFIGSTATISTICS
	      total_memXferCMBToPrimary++;
#endif
	      batchCommandEnd();
#ifndef VERBOSEDEBUG
	      newSize = newSize + dumpStitchSizeLower + dumpStitchSizeUpper;
#endif
	    }
	  }

#ifndef VERBOSEDEBUG
	  cerr << "SCHED: =====> YOU JUST SAVED DUMPING AN EXTRA: " << 
            (originalSize-newSize) << endl;
#endif
	}
      }
    }

    for (j = 0; j < loadBlocks_count; j++) {
      ScoreSegmentLoadDumpBlock *loadBlock = &(loadBlocks[j]);

      if (!(loadBlock->bufferSizeDependsOnOwnerAddrs)) {
	batchCommandBegin();
	memXferPrimaryToCMB(loadBlock->buffer,
			    i,
			    loadBlock->blockStart,
			    loadBlock->bufferSize);
#if KEEPRECONFIGSTATISTICS
	total_memXferPrimaryToCMB++;
#endif
	batchCommandEnd();
      } else {
	// CURRENTLY THE ONLY SEGMENTS THAT HAVE THEIR DATA DUMPED/LOADED
	// VIA bufferSizeDependsOnOwnerAddrs ARE STITCH BUFFERS!
	ScoreSegmentStitch *loadStitch = 
	  (ScoreSegmentStitch *) loadBlock->owner;
	unsigned int loadStitchReadAddr = loadStitch->sched_readAddr;
	unsigned int loadStitchWriteAddr = loadStitch->sched_writeAddr;
	
#if VERBOSEDEBUG
	  unsigned int originalSize =
	    loadStitch->length() * 
            (SCORE_ALIGN_BITWIDTH_TO_8(loadStitch->width())/8);
	  unsigned int newSize = 0;
#endif

	// ignore if stitch buffer is empty!
	if (loadStitchReadAddr != loadStitchWriteAddr) {
	  if (loadStitchReadAddr < loadStitchWriteAddr) {
	    // the valid block is 1 contiguous section!
	    unsigned int loadStitchSize = 
	      (loadStitchWriteAddr - loadStitchReadAddr)*
	      (SCORE_ALIGN_BITWIDTH_TO_8(loadStitch->width())/8);
	    unsigned int loadStitchStart =
	      loadBlock->blockStart +
	      (loadStitchReadAddr*(SCORE_ALIGN_BITWIDTH_TO_8(loadStitch->width())/8));
	    void *loadStitchBuffer =
	      (void *) (((unsigned long long *) loadBlock->buffer) +
			loadStitchReadAddr);

	    batchCommandBegin();
	    memXferPrimaryToCMB(loadStitchBuffer,
				i,
				loadStitchStart,
				loadStitchSize);
#if KEEPRECONFIGSTATISTICS
	    total_memXferPrimaryToCMB++;
#endif
	    batchCommandEnd();
#if VERBOSEDEBUG
	    newSize = newSize + loadStitchSize;
#endif
	  } else {
	    // the valid block is actually 2 contiguous sections!
	    unsigned int loadStitchSizeLower = 
	      (loadStitchWriteAddr - 0)*
	      (SCORE_ALIGN_BITWIDTH_TO_8(loadStitch->width())/8);
	    unsigned int loadStitchStartLower =
	      loadBlock->blockStart +
	      (0*(SCORE_ALIGN_BITWIDTH_TO_8(loadStitch->width())/8));
	    void *loadStitchBufferLower =
	      (void *) (((unsigned long long *) loadBlock->buffer) +
			0);
	    unsigned int loadStitchSizeUpper = 
	      (loadStitch->length() - loadStitchReadAddr)*
	      (SCORE_ALIGN_BITWIDTH_TO_8(loadStitch->width())/8);
	    unsigned int loadStitchStartUpper =
	      loadBlock->blockStart +
	      (loadStitchReadAddr*(SCORE_ALIGN_BITWIDTH_TO_8(loadStitch->width())/8));
	    void *loadStitchBufferUpper =
	      (void *) (((unsigned long long *) loadBlock->buffer) +
			loadStitchReadAddr);

	    batchCommandBegin();
	    memXferPrimaryToCMB(loadStitchBufferLower,
				i,
				loadStitchStartLower,
				loadStitchSizeLower);
#if KEEPRECONFIGSTATISTICS
	    total_memXferPrimaryToCMB++;
#endif
	    batchCommandEnd();

	    batchCommandBegin();
	    memXferPrimaryToCMB(loadStitchBufferUpper,
				i,
				loadStitchStartUpper,
				loadStitchSizeUpper);
#if KEEPRECONFIGSTATISTICS
	    total_memXferPrimaryToCMB++;
#endif
	    batchCommandEnd();
#if VERBOSEDEBUG
	    newSize = newSize + loadStitchSizeLower + loadStitchSizeUpper;
#endif
	  }
	}

#if VERBOSEDEBUG
	  cerr << "SCHED: =====> YOU JUST SAVED LOADING AN EXTRA: " << 
            (originalSize-newSize) << endl;
#endif
      }
    }

    arrayCMB[i].segmentTable->dumpBlocks_count = 0;
    arrayCMB[i].segmentTable->loadBlocks_count = 0;
  }

  // take care of any faulted CMBs by updating the TRA and PBO and MAX.
  for (i = 0; i < SCORECUSTOMLIST_LENGTH(faultedMemSegList); i++) {
    ScoreSegment *currentSegment;

    SCORECUSTOMLIST_ITEMAT(faultedMemSegList, i, currentSegment);

    // change the TRA and PBO and MAX for the segment.
    batchCommandBegin();
    changeSegmentTRAandPBOandMAX(currentSegment);
#if KEEPRECONFIGSTATISTICS
    total_changeSegmentTRAandPBOandMAX++;
#endif
    batchCommandEnd();
  }

  // go through to each physical page and CMB, and, if a new page or
  // CMB is to be loaded, load it.
  for (i = 0; i < numPhysicalCMB; i++) {
    if ((arrayCMB[i].active != arrayCMB[i].scheduled) &&
        (arrayCMB[i].scheduled != NULL)) {
      arrayCMB[i].active = arrayCMB[i].scheduled;
      arrayCMB[i].active->sched_isResident = 1;
      arrayCMB[i].active->sched_residentLoc = i;
      arrayCMB[i].actual = arrayCMB[i].active;

      // load the config/pointers.
      batchCommandBegin();
      setSegmentConfigPointers(arrayCMB[i].scheduled);
#if KEEPRECONFIGSTATISTICS
      total_setSegmentConfigPointers++;
#endif
      batchCommandEnd();
    }
  }
  // FIX ME! IS IT OKAY TO LOAD IN THE FIFO AND STATE BEFORE THE CONFIG??
  while ((loadPageConfig_todo.length() != 1) ||
	 (loadPageState_todo.length() != 1) ||
	 (loadPageFIFO_todo.length() != 1) ||
	 (loadSegmentFIFO_todo.length() != 1)) {
    list<unsigned int> busyCPs, busyCMBs;
    ScorePage *currentPage = NULL;
    ScoreSegment *currentSegment = NULL;

    batchCommandBegin();

    currentPage = loadPageConfig_todo.pop();
    while (currentPage != NULL) {
      unsigned int currentLoc = currentPage->sched_residentLoc;
      unsigned int cachedLoc = 
	currentPage->sched_cachedSegmentBlock->parentTable->loc;

      if (!(arrayCPBusy[currentLoc]) &&
	  !(arrayCMBBusy[cachedLoc])) {
	arrayCP[currentLoc].active = arrayCP[currentLoc].scheduled;
	arrayCP[currentLoc].active->sched_isResident = 1;
	arrayCP[currentLoc].actual = arrayCP[currentLoc].active;
	
	arrayCPBusy[currentLoc] = 1;
	arrayCMBBusy[cachedLoc] = 1;
	busyCPs.append(currentLoc);
	busyCMBs.append(cachedLoc);

	loadPageConfig(currentPage);
#if KEEPRECONFIGSTATISTICS
	total_loadPageConfig++;
#endif
      } else {
	loadPageConfig_todo.append(currentPage);
      }
      currentPage = loadPageConfig_todo.pop();
    }
    loadPageConfig_todo.append(NULL);

    currentPage = loadPageState_todo.pop();
    while (currentPage != NULL) {
      unsigned int currentLoc = currentPage->sched_residentLoc;
      unsigned int cachedLoc = 
	currentPage->sched_cachedSegmentBlock->parentTable->loc;

      if (!(arrayCPBusy[currentLoc]) &&
	  !(arrayCMBBusy[cachedLoc])) {
	arrayCPBusy[currentLoc] = 1;
	arrayCMBBusy[cachedLoc] = 1;
	busyCPs.append(currentLoc);
	busyCMBs.append(cachedLoc);

	loadPageState(currentPage);
#if KEEPRECONFIGSTATISTICS
	total_loadPageState++;
#endif
      } else {
	loadPageState_todo.append(currentPage);
      }
      currentPage = loadPageState_todo.pop();
    }
    loadPageState_todo.append(NULL);

    currentPage = loadPageFIFO_todo.pop();
    while (currentPage != NULL) {
      unsigned int currentLoc = currentPage->sched_residentLoc;

      // if FIFO data exists, load it. otherwise, initialize the FIFOs.
      if (currentPage->sched_isFIFOBufferValid) {
	unsigned int cachedLoc = 
	  currentPage->sched_cachedSegmentBlock->parentTable->loc;

	if (!(arrayCPBusy[currentLoc]) &&
	    !(arrayCMBBusy[cachedLoc])) {
	  arrayCPBusy[currentLoc] = 1;
	  arrayCMBBusy[cachedLoc] = 1;
	  busyCPs.append(currentLoc);
	  busyCMBs.append(cachedLoc);
	    
	  loadPageFIFO(currentPage);
#if KEEPRECONFIGSTATISTICS
	  total_loadPageFIFO++;
#endif
	} else {
	  loadPageFIFO_todo.append(currentPage);
	}
      } else {
	currentPage->sched_isFIFOBufferValid = 1;
	// FIX ME! DO NOT KNOW WHAT TO DO IN ORDER TO INITIALIZE FIFOS!
      }
      currentPage = loadPageFIFO_todo.pop();
    }
    loadPageFIFO_todo.append(NULL);

    currentSegment = loadSegmentFIFO_todo.pop();
    while (currentSegment != NULL) {
      unsigned int currentLoc = currentSegment->sched_residentLoc;

      // if FIFO data exists, load it. otherwise, initialize the FIFOs.
      if (currentSegment->sched_isFIFOBufferValid) {
	unsigned int cachedLoc = 
	  currentSegment->sched_cachedSegmentBlock->parentTable->loc;

	if (!(arrayCMBBusy[currentLoc]) &&
	    !(arrayCMBBusy[cachedLoc])) {
	  arrayCMBBusy[currentLoc] = 1;
	  arrayCMBBusy[cachedLoc] = 1;
	  busyCMBs.append(currentLoc);
	  busyCMBs.append(cachedLoc);
	    
	  loadSegmentFIFO(currentSegment);
#if KEEPRECONFIGSTATISTICS
	  total_loadSegmentFIFO++;
#endif
	} else {
	  loadSegmentFIFO_todo.append(currentSegment);
	}
      } else {
	currentSegment->sched_isFIFOBufferValid = 1;
	// FIX ME! DO NOT KNOW WHAT TO DO IN ORDER TO INITIALIZE FIFOS!
      }
      currentSegment = loadSegmentFIFO_todo.pop();
    }
    loadSegmentFIFO_todo.append(NULL);

    batchCommandEnd();

    // clear the busy flags.
    {
      unsigned int busyLoc;

      while (busyCPs.length() != 0) {
	busyLoc = busyCPs.pop();

	arrayCPBusy[busyLoc] = 0;
      }
      while (busyCMBs.length() != 0) {
	busyLoc = busyCMBs.pop();

	arrayCMBBusy[busyLoc] = 0;
      }
    }
  }

  for (i = 0; i < SCORECUSTOMLIST_LENGTH(configChangedStitchSegList);
       i++) {
    ScoreSegment *currentSegment;

    SCORECUSTOMLIST_ITEMAT(configChangedStitchSegList, i, currentSegment);

    // change the segment mode.
    batchCommandBegin();
    changeSegmentMode(currentSegment);
#if KEEPRECONFIGSTATISTICS
    total_changeSegmentMode++;
#endif
    batchCommandEnd();

    if ((currentSegment->sched_old_mode == SCORE_CMB_SEQSINK) &&
        (currentSegment->sched_mode == SCORE_CMB_SEQSRCSINK)) {
      batchCommandBegin();
      resetSegmentDoneFlag(currentSegment);
#if KEEPRECONFIGSTATISTICS
      total_resetSegmentDoneFlag++;
#endif
      batchCommandEnd();
    }
  }

  // reconnect the streams.
  // FIX ME! SHOULD FIND A BETTER WAY TO DO THIS! RIGHT NOW, I AM JUST
  // RECONNECTING ALL STREAMS TO ENSURE CORRECTNESS.
  batchCommandBegin();
  for (i = 0; i < numPhysicalCP; i++) {
    ScorePage *currentPage = arrayCP[i].active;

    if ((currentPage != NULL) && !(currentPage->sched_isDone)) {
      unsigned int numOutputs = currentPage->getOutputs();
      unsigned int j;

      for (j = 0; j < numOutputs; j++) {
	SCORE_STREAM outputStream = currentPage->getSchedOutput(j);

	ScoreGraphNode *outputNode = outputStream->sched_sink;
	unsigned int outputNodeInputNumber = outputStream->sched_snkNum;

	if (outputStream->sched_sinkIsDone) {
	  // FIX ME! MUST FIGURE OUT HOW TO INDICATE A STREAM THAT CONNECTS
	  // TO NOWHERE BUT MUST ALWAYS ALLOW STREAM_WRITES.
	  outputStream->syncSchedToReal();
	} else if (outputStream->sched_snkFunc == STREAM_OPERATOR_TYPE) {
	  // FIX ME! MUST FIGURE OUT HOW TO CONNECT ARRAY<->PROCESSOR.
	  outputStream->syncSchedToReal();
	} else {
	  connectStream(currentPage, j, outputNode, outputNodeInputNumber);
#if KEEPRECONFIGSTATISTICS
	  total_connectStream++;
#endif
	}
      }
    }
  }
  for (i = 0; i < numPhysicalCMB; i++) {
    ScoreSegment *currentSegment = arrayCMB[i].active;

    // NOTE: Since stitch buffers might have outputs, but may be in a
    //       mode such that we do not want to connect outputs, then
    //       check for that.
    if ((currentSegment != NULL) &&
	(!(currentSegment->sched_isStitch) ||
	 !(currentSegment->mode == SCORE_CMB_SEQSINK))) {
      unsigned int numOutputs = currentSegment->getOutputs();
      unsigned int j;

      for (j = 0; j < numOutputs; j++) {
	SCORE_STREAM outputStream = currentSegment->getSchedOutput(j);

	ScoreGraphNode *outputNode = outputStream->sched_sink;
	unsigned int outputNodeInputNumber = outputStream->sched_snkNum;
	
	if (outputStream->sched_sinkIsDone) {
	  // FIX ME! MUST FIGURE OUT HOW TO INDICATE A STREAM THAT CONNECTS
	  // TO NOWHERE BUT MUST ALWAYS ALLOW STREAM_WRITES.
	  outputStream->syncSchedToReal();
	} else if (outputStream->sched_snkFunc == STREAM_OPERATOR_TYPE) {
	  // FIX ME! MUST FIGURE OUT HOW TO CONNECT ARRAY<->PROCESSOR.
	  outputStream->syncSchedToReal();
	} else {
	  connectStream(currentSegment, j, outputNode, 
			outputNodeInputNumber);
#if KEEPRECONFIGSTATISTICS
	  total_connectStream++;
#endif
	}
      }
    }
  }
  // FIX ME! MUST FIGURE OUT HOW TO MAKE A DISCONNECT INPUT CONNECTED TO
  // NOTHING!
  for (i = 0; i < SCORECUSTOMLIST_LENGTH(processorIStreamList); i++) {
    ScoreStream *outputStream;
    // ScoreGraphNode *outputNode;
    // unsigned int outputNodeInputNumber;

    SCORECUSTOMLIST_ITEMAT(processorIStreamList, i, outputStream);
    // outputNode = outputStream->sched_sink;
    // outputNodeInputNumber = outputStream->sched_snkNum;

    // FIX ME! SHOULD TAKE CARE OF INPUTS COMING FROM THE PROCESSOR!
    // connectStream(NULL, 0, outputNode, outputNodeInputNumber); ????
    outputStream->syncSchedToReal();
  }
  batchCommandEnd();

  // start every page and CMB.
  // make sure that any done pages left on the array are not started again!
  batchCommandBegin();
  for (i = 0; i < numPhysicalCP; i++) {
    if ((arrayCP[i].active != NULL) &&
	!(arrayCP[i].active->sched_isDone)) {
      startPage(arrayCP[i].active);
#if KEEPRECONFIGSTATISTICS
      total_startPage++;
#endif
    }
  }
  for (i = 0; i < numPhysicalCMB; i++) {
    if (arrayCMB[i].active != NULL) {
      startSegment(arrayCMB[i].active);
#if KEEPRECONFIGSTATISTICS
      total_startSegment++;
#endif
    }
  }
  batchCommandEnd();

  // clean up lists.
  SCORECUSTOMLIST_CLEAR(scheduledPageList);
  SCORECUSTOMLIST_CLEAR(scheduledMemSegList);
  SCORECUSTOMLIST_CLEAR(removedPageList);
  SCORECUSTOMLIST_CLEAR(removedMemSegList);
  SCORECUSTOMLIST_CLEAR(configChangedStitchSegList);
  SCORECUSTOMLIST_CLEAR(faultedMemSegList);

#if 0 && DOPROFILING
  endClock = threadCounter->ScoreThreadSchCounterRead();
  diffClock = endClock - startClock;
  if (diffClock < min_issueReconfigCommands) {
    min_issueReconfigCommands = diffClock;
  }
  if (diffClock > max_issueReconfigCommands) {
    max_issueReconfigCommands = diffClock;
  }
  total_issueReconfigCommands = total_issueReconfigCommands + diffClock;
  cerr << "   issueReconfigCommands() ==> " << 
    diffClock <<
    " cycle(s)" << endl;
#endif
}

