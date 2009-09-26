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
// $Revision: 1.20 $
//
//////////////////////////////////////////////////////////////////////////////

#include <iostream>
#include "ScoreCustomLinkedList.h"
#include "ScoreSegmentTable.h"
#include "ScoreConfig.h"


using std::cout;
using std::endl;
using std::cerr;

///////////////////////////////////////////////////////////////////////////////
// ScoreSegmentBlock::ScoreSegmentBlock:
//   Constructor for ScoreSegmentBlock.
//   Initializes all internal structures.
//
// Parameters: None.
//
// Return value: None.
///////////////////////////////////////////////////////////////////////////////
ScoreSegmentBlock::ScoreSegmentBlock() {
  type = 0;
  start = 0;
  size = 0;
  owner = NULL;
  parentTable = NULL;
  parentBlockIndex = 0;
  childBlockStartIndex = 0;
  childBlockSize = 0;
  childBlocksInUse = 0;
  freeListItem = SCORECUSTOMLINKEDLIST_NULL;
  unavailableListItem = SCORECUSTOMLINKEDLIST_NULL;
  usedListItem = SCORECUSTOMLINKEDLIST_NULL;
  cachedListItem = SCORECUSTOMLINKEDLIST_NULL;
}


///////////////////////////////////////////////////////////////////////////////
// ScoreSegmentBlock::~ScoreSegmentBlock:
//   Destructor for ScoreSegmentBlock.
//   Cleans up all internal structures.
//
// Parameters: None.
//
// Return value: None.
///////////////////////////////////////////////////////////////////////////////
ScoreSegmentBlock::~ScoreSegmentBlock() {
  // do nothing!
}


///////////////////////////////////////////////////////////////////////////////
// ScoreSegmentLoadDumpBlock::ScoreSegmentLoadDumpBlock:
//   Constructor for ScoreSegmentLoadDumpBlock.
//   Initializes all internal structures.
//
// Parameters: None.
//
// Return value: None.
///////////////////////////////////////////////////////////////////////////////
ScoreSegmentLoadDumpBlock::ScoreSegmentLoadDumpBlock() {
  owner = NULL;
  blockStart = 0;
  buffer = NULL;
  bufferSize = 0;
  bufferSizeDependsOnOwnerAddrs = 0;
}


///////////////////////////////////////////////////////////////////////////////
// ScoreSegmentLoadDumpBlock::~ScoreSegmentLoadDumpBlock:
//   Destructor for ScoreSegmentLoadDumpBlock.
//   Cleans up all internal structures.
//
// Parameters: None.
//
// Return value: None.
///////////////////////////////////////////////////////////////////////////////
ScoreSegmentLoadDumpBlock::~ScoreSegmentLoadDumpBlock() {
  // do nothing!
}


///////////////////////////////////////////////////////////////////////////////
// ScoreSegmentTable::ScoreSegmentTable:
//   Constructor for ScoreSegmentTable.
//   Initializes all internal structures.
//
// Parameters:
//   newLoc: memory segment physical location.
//   newCMBSize: CMB physical size.
//
// Return value: None.
///////////////////////////////////////////////////////////////////////////////
ScoreSegmentTable::ScoreSegmentTable(unsigned int newLoc,
				     unsigned int newCMBSize) {
  unsigned int i;

  loc = newLoc;
  cmbSize = newCMBSize;

  {
    unsigned int numLevel0InCMB = 
      cmbSize / SCORE_SEGMENTTABLE_LEVEL0SIZE;
    unsigned int sizeLevel0CruftInCMB = 
      cmbSize % SCORE_SEGMENTTABLE_LEVEL0SIZE;
    unsigned int numLevel1InLevel0 =
      SCORE_SEGMENTTABLE_LEVEL0SIZE / SCORE_SEGMENTTABLE_LEVEL1SIZE;
    unsigned int sizeLevel1CruftInLevel0 =
      SCORE_SEGMENTTABLE_LEVEL0SIZE % SCORE_SEGMENTTABLE_LEVEL1SIZE;
    unsigned int numLevel1InLevel0CruftInCMB =
      sizeLevel0CruftInCMB / SCORE_SEGMENTTABLE_LEVEL1SIZE;
    unsigned int sizeLevel1CruftInLevel0CruftInCMB =
      sizeLevel0CruftInCMB % SCORE_SEGMENTTABLE_LEVEL1SIZE;
    unsigned int actualNumLevel0InCMB;
    unsigned int actualNumLevel1InLevel0;
    unsigned int totalLevel0;
    unsigned int totalLevel1;

    if (sizeLevel0CruftInCMB == 0) {
      actualNumLevel0InCMB = numLevel0InCMB;
    } else {
      actualNumLevel0InCMB = numLevel0InCMB + 1;
    }
    if (sizeLevel1CruftInLevel0 == 0) {
      actualNumLevel1InLevel0 = numLevel1InLevel0;
    } else {
      actualNumLevel1InLevel0 = numLevel1InLevel0 + 1;
    }

    totalLevel0 = actualNumLevel0InCMB;
    if (sizeLevel1CruftInLevel0CruftInCMB == 0) {
      totalLevel1 = (actualNumLevel1InLevel0 * numLevel0InCMB) +
	  numLevel1InLevel0CruftInCMB;
    } else {
      totalLevel1 = (actualNumLevel1InLevel0 * numLevel0InCMB) +
	  numLevel1InLevel0CruftInCMB + 1;
    }

    // consider the worst-case load/dump requirements!
    // NOTE: for loads: all of the level1 blocks each with config/state/fifo.
    // NOTE: for dumps: all of the level1 blocks each with state/fifo.
    loadBlocks = new ScoreSegmentLoadDumpBlock[totalLevel1*3];
    loadBlocks_count = 0;
    loadBlocks_capacity = totalLevel1*3;
    dumpBlocks = new ScoreSegmentLoadDumpBlock[totalLevel1*2];
    dumpBlocks_count = 0;
    dumpBlocks_capacity = totalLevel1*2;

    freeList_Level0Blocks =
      new ScoreCustomLinkedList<ScoreSegmentBlock *>(totalLevel0);
    freeList_Level1Blocks =
      new ScoreCustomLinkedList<ScoreSegmentBlock *>(totalLevel1);
    unavailableList_Level0Blocks =
      new ScoreCustomLinkedList<ScoreSegmentBlock *>(totalLevel0);
    unavailableList_Level1Blocks =
      new ScoreCustomLinkedList<ScoreSegmentBlock *>(totalLevel1);
    usedList_Level0Blocks =
      new ScoreCustomLinkedList<ScoreSegmentBlock *>(totalLevel0);
    usedList_Level1Blocks =
      new ScoreCustomLinkedList<ScoreSegmentBlock *>(totalLevel1);
    cachedList_Level0Blocks =
      new ScoreCustomLinkedList<ScoreSegmentBlock *>(totalLevel0);
    cachedList_Level1Blocks =
      new ScoreCustomLinkedList<ScoreSegmentBlock *>(totalLevel1);

    level0Blocks = new ScoreSegmentBlock[totalLevel0];
    for (i = 0; i < totalLevel0; i++) {
      if ((i == (totalLevel0 - 1)) &&
	  (sizeLevel0CruftInCMB != 0)) {
	level0Blocks[i].type = SCORE_SEGMENTBLOCK_LEVEL0_CRUFT;
      } else {
	level0Blocks[i].type = SCORE_SEGMENTBLOCK_LEVEL0_FREE;
	SCORECUSTOMLINKEDLIST_APPEND(freeList_Level0Blocks, 
				     &(level0Blocks[i]),
				     level0Blocks[i].freeListItem);
      }
      level0Blocks[i].start = SCORE_SEGMENTTABLE_LEVEL0SIZE * i;
      if ((i == (totalLevel0 - 1)) &&
	  (sizeLevel0CruftInCMB != 0)) {
	level0Blocks[i].size = sizeLevel0CruftInCMB;
      } else {
	level0Blocks[i].size = SCORE_SEGMENTTABLE_LEVEL0SIZE;
      }
      level0Blocks[i].owner = NULL;
      level0Blocks[i].parentTable = this;
      level0Blocks[i].childBlockStartIndex = actualNumLevel1InLevel0 * i;
      // NOTE: We purposefully do not want to include the cruft level1 block.
      level0Blocks[i].childBlockSize = numLevel1InLevel0;
    }
    level1Blocks = new ScoreSegmentBlock[totalLevel1];
    for (i = 0; i < totalLevel1; i++) {
      if ((((i + 1) % actualNumLevel1InLevel0) == 0) &&
	  (sizeLevel1CruftInLevel0 != 0)) {
	level1Blocks[i].type = SCORE_SEGMENTBLOCK_LEVEL1_CRUFT;
      } else if ((i == (totalLevel1 - 1)) &&
		 (sizeLevel1CruftInLevel0CruftInCMB != 0)) {
	level1Blocks[i].type = SCORE_SEGMENTBLOCK_LEVEL1_CRUFT;
      } else {
	level1Blocks[i].type = SCORE_SEGMENTBLOCK_LEVEL1_UNAVAILABLE;
	SCORECUSTOMLINKEDLIST_APPEND(unavailableList_Level1Blocks,
				     &(level1Blocks[i]),
				     level1Blocks[i].unavailableListItem);
      }
      if (i == 0) {
	level1Blocks[i].start = 0;
      } else {
	level1Blocks[i].start = level1Blocks[i-1].start + 
	  level1Blocks[i-1].size;
      }
      if (((i % numLevel1InLevel0) == 0) && (i != 0) &&
	  (sizeLevel1CruftInLevel0 != 0)) {
	level1Blocks[i].size = sizeLevel1CruftInLevel0;
      } else {
	level1Blocks[i].size = SCORE_SEGMENTTABLE_LEVEL1SIZE;
      }
      level1Blocks[i].owner = NULL;
      level1Blocks[i].parentTable = this;
      level1Blocks[i].parentBlockIndex = (i / actualNumLevel1InLevel0);
      if ((level0Blocks[level1Blocks[i].parentBlockIndex].type ==
	   SCORE_SEGMENTBLOCK_LEVEL0_CRUFT) &&
	  (level1Blocks[i].type ==
	   SCORE_SEGMENTBLOCK_LEVEL1_UNAVAILABLE)) {
	SCORECUSTOMLINKEDLIST_DELITEM(unavailableList_Level1Blocks,
				      level1Blocks[i].unavailableListItem);
	level1Blocks[i].type = SCORE_SEGMENTBLOCK_LEVEL1_FREE;
	SCORECUSTOMLINKEDLIST_APPEND(freeList_Level1Blocks,
				     &(level1Blocks[i]),
				     level1Blocks[i].freeListItem);
      }
    }
  }
}


///////////////////////////////////////////////////////////////////////////////
// ScoreSegmentTable::~ScoreSegmentTable:
//   Destructor for ScoreSegmentTable.
//   Cleans up all internal structures.
//
// Parameters: None.
//
// Return value: None.
///////////////////////////////////////////////////////////////////////////////
ScoreSegmentTable::~ScoreSegmentTable() {
  delete(level0Blocks);
  delete(level1Blocks);
  delete(loadBlocks);
  delete(dumpBlocks);
  delete(freeList_Level0Blocks);
  delete(freeList_Level1Blocks);
  delete(unavailableList_Level0Blocks);
  delete(unavailableList_Level1Blocks);
  delete(usedList_Level0Blocks);
  delete(usedList_Level1Blocks);
  delete(cachedList_Level0Blocks);
  delete(cachedList_Level1Blocks);
}


///////////////////////////////////////////////////////////////////////////////
// ScoreSegmentTable::allocateLevel0Block:
//   Attempt to allocate a free level0 block.
//   It will look at freeList_Level0Blocks for free blocks.
//
// Parameters: owner: the future owner of the block.
//
// Return value:
//   If successful, a pointer to the block; else, NULL.
///////////////////////////////////////////////////////////////////////////////
ScoreSegmentBlock *ScoreSegmentTable::allocateLevel0Block(
  ScoreGraphNode *owner) {
  if (SCORECUSTOMLINKEDLIST_ISEMPTY(freeList_Level0Blocks)) {
    return(NULL);
  } else {
    ScoreSegmentBlock *freeBlock;

    SCORECUSTOMLINKEDLIST_POP(freeList_Level0Blocks, freeBlock);

    freeBlock->type = SCORE_SEGMENTBLOCK_LEVEL0_USED;
    freeBlock->owner = owner;
    freeBlock->freeListItem = SCORECUSTOMLINKEDLIST_NULL;

    SCORECUSTOMLINKEDLIST_APPEND(usedList_Level0Blocks, freeBlock,
				 freeBlock->usedListItem);

    return(freeBlock);
  }
}


///////////////////////////////////////////////////////////////////////////////
// ScoreSegmentTable::allocateLevel1Block:
//   Attempt to allocate a free level1 block.
//   It will look at freeList_Level1Blocks for free blocks. If no such block,
//     exists, it will look at freeList_Level0Blocks for free blocks. Then,
//     it will break up that level0 block and allocate a level1 block.
//
// Parameters: owner: the future owner of the block.
//
// Return value:
//   If successful, a pointer to the block; else, NULL.
///////////////////////////////////////////////////////////////////////////////
ScoreSegmentBlock *ScoreSegmentTable::allocateLevel1Block(
  ScoreGraphNode *owner) {
  if (SCORECUSTOMLINKEDLIST_ISEMPTY(freeList_Level1Blocks)) {
    if (SCORECUSTOMLINKEDLIST_ISEMPTY(freeList_Level0Blocks)) {
      return(NULL);
    } else if ((SCORECUSTOMLINKEDLIST_LENGTH(freeList_Level0Blocks)+
		SCORECUSTOMLINKEDLIST_LENGTH(cachedList_Level0Blocks)+
		SCORECUSTOMLINKEDLIST_LENGTH(usedList_Level0Blocks)) == 1) {
      // to preserve the guarantee that we always have a level0 segment block
      // in each CMB for a resident segment.
      return(NULL);
    } else {
      ScoreSegmentBlock *freeLevel0Block;
      unsigned int childBlockEndIndex;
      unsigned int i;

      SCORECUSTOMLINKEDLIST_POP(freeList_Level0Blocks, freeLevel0Block);
      childBlockEndIndex =
	freeLevel0Block->childBlockStartIndex +
	freeLevel0Block->childBlockSize - 1;

      freeLevel0Block->type = SCORE_SEGMENTBLOCK_LEVEL0_UNAVAILABLE;
      freeLevel0Block->freeListItem = SCORECUSTOMLINKEDLIST_NULL;

      SCORECUSTOMLINKEDLIST_APPEND(unavailableList_Level0Blocks,
				   freeLevel0Block,
				   freeLevel0Block->unavailableListItem);

      for (i = freeLevel0Block->childBlockStartIndex;
	   i < childBlockEndIndex; i++) {
	ScoreSegmentBlock *convertedBlock = &(level1Blocks[i]);

	SCORECUSTOMLINKEDLIST_DELITEM(unavailableList_Level1Blocks,
				      convertedBlock->unavailableListItem);

	convertedBlock->type = SCORE_SEGMENTBLOCK_LEVEL1_FREE;
	
	SCORECUSTOMLINKEDLIST_APPEND(freeList_Level1Blocks, convertedBlock,
				     convertedBlock->freeListItem);
      }

      {
	ScoreSegmentBlock *freeBlock = &(level1Blocks[childBlockEndIndex]);

	SCORECUSTOMLINKEDLIST_DELITEM(unavailableList_Level1Blocks,
				      freeBlock->unavailableListItem);

	freeBlock->type = SCORE_SEGMENTBLOCK_LEVEL1_USED;
	freeBlock->owner = owner;
	
	SCORECUSTOMLINKEDLIST_APPEND(usedList_Level1Blocks, freeBlock,
				     freeBlock->usedListItem);

	level0Blocks[freeBlock->parentBlockIndex].childBlocksInUse++;

	return(freeBlock);
      }
    }
  } else {
    ScoreSegmentBlock *freeBlock;

    SCORECUSTOMLINKEDLIST_POP(freeList_Level1Blocks, freeBlock);

    freeBlock->type = SCORE_SEGMENTBLOCK_LEVEL1_USED;
    freeBlock->owner = owner;
    freeBlock->freeListItem = SCORECUSTOMLINKEDLIST_NULL;

    SCORECUSTOMLINKEDLIST_APPEND(usedList_Level1Blocks, freeBlock,
				 freeBlock->usedListItem);

    level0Blocks[freeBlock->parentBlockIndex].childBlocksInUse++;

    return(freeBlock);
  }
}


void ScoreSegmentTable::freeCachedLevel0Block(ScoreSegmentBlock *block) {
  block->type = SCORE_SEGMENTBLOCK_LEVEL0_FREE;
  block->owner = NULL;

  SCORECUSTOMLINKEDLIST_DELITEM(cachedList_Level0Blocks,
				block->cachedListItem);

  SCORECUSTOMLINKEDLIST_APPEND(freeList_Level0Blocks, block,
			       block->freeListItem);
}


void ScoreSegmentTable::freeCachedLevel1Block(ScoreSegmentBlock *block) {
  block->type = SCORE_SEGMENTBLOCK_LEVEL1_FREE;
  block->owner = NULL;

  SCORECUSTOMLINKEDLIST_DELITEM(cachedList_Level1Blocks,
				block->cachedListItem);

  SCORECUSTOMLINKEDLIST_APPEND(freeList_Level1Blocks, block,
			       block->freeListItem);

  if (level0Blocks[block->parentBlockIndex].type !=
      SCORE_SEGMENTBLOCK_LEVEL0_CRUFT) {
    level0Blocks[block->parentBlockIndex].childBlocksInUse--;

    if (level0Blocks[block->parentBlockIndex].childBlocksInUse == 0) {
      ScoreSegmentBlock *parentBlock = 
	&(level0Blocks[block->parentBlockIndex]);
      unsigned int childBlockEndIndex =
	parentBlock->childBlockStartIndex +
	parentBlock->childBlockSize;
      unsigned int i;

      parentBlock->type = SCORE_SEGMENTBLOCK_LEVEL0_FREE;
      
      SCORECUSTOMLINKEDLIST_DELITEM(unavailableList_Level0Blocks,
				    parentBlock->unavailableListItem);

      SCORECUSTOMLINKEDLIST_APPEND(freeList_Level0Blocks, parentBlock,
				   parentBlock->freeListItem);

      for (i = parentBlock->childBlockStartIndex;
	   i < childBlockEndIndex; i++) {
	ScoreSegmentBlock *convertedBlock = &(level1Blocks[i]);

	SCORECUSTOMLINKEDLIST_DELITEM(freeList_Level1Blocks,
				      convertedBlock->freeListItem);

	convertedBlock->type = SCORE_SEGMENTBLOCK_LEVEL1_UNAVAILABLE;
	
	SCORECUSTOMLINKEDLIST_APPEND(unavailableList_Level1Blocks,
				     convertedBlock,
				     convertedBlock->unavailableListItem);
      }
    }
  }
}


void ScoreSegmentTable::freeCachedLevel1Block_nomerge(
  ScoreSegmentBlock *block) {
  block->type = SCORE_SEGMENTBLOCK_LEVEL1_FREE;
  block->owner = NULL;

  SCORECUSTOMLINKEDLIST_DELITEM(cachedList_Level1Blocks,
				block->cachedListItem);

  SCORECUSTOMLINKEDLIST_APPEND(freeList_Level1Blocks, block,
			       block->freeListItem);

  if (level0Blocks[block->parentBlockIndex].type !=
      SCORE_SEGMENTBLOCK_LEVEL0_CRUFT) {
    level0Blocks[block->parentBlockIndex].childBlocksInUse--;
  }
}


void ScoreSegmentTable::freeUsedLevel0Block(ScoreSegmentBlock *block) {
  block->type = SCORE_SEGMENTBLOCK_LEVEL0_FREE;
  block->owner = NULL;

  SCORECUSTOMLINKEDLIST_DELITEM(usedList_Level0Blocks,
				block->usedListItem);

  SCORECUSTOMLINKEDLIST_APPEND(freeList_Level0Blocks, block,
			       block->freeListItem);
}


void ScoreSegmentTable::freeUsedLevel1Block(ScoreSegmentBlock *block) {
  block->type = SCORE_SEGMENTBLOCK_LEVEL1_FREE;
  block->owner = NULL;

  SCORECUSTOMLINKEDLIST_DELITEM(usedList_Level1Blocks, block->usedListItem);

  SCORECUSTOMLINKEDLIST_APPEND(freeList_Level1Blocks, block,
			       block->freeListItem);

  if (level0Blocks[block->parentBlockIndex].type !=
      SCORE_SEGMENTBLOCK_LEVEL0_CRUFT) {
    level0Blocks[block->parentBlockIndex].childBlocksInUse--;

    if (level0Blocks[block->parentBlockIndex].childBlocksInUse == 0) {
      ScoreSegmentBlock *parentBlock = 
	&(level0Blocks[block->parentBlockIndex]);
      unsigned int childBlockEndIndex =
	parentBlock->childBlockStartIndex +
	parentBlock->childBlockSize;
      unsigned int i;

      parentBlock->type = SCORE_SEGMENTBLOCK_LEVEL0_FREE;
      
      SCORECUSTOMLINKEDLIST_DELITEM(unavailableList_Level0Blocks,
				    parentBlock->unavailableListItem);

      SCORECUSTOMLINKEDLIST_APPEND(freeList_Level0Blocks, parentBlock,
				   parentBlock->freeListItem);
      
      for (i = parentBlock->childBlockStartIndex;
	   i < childBlockEndIndex; i++) {
	ScoreSegmentBlock *convertedBlock = &(level1Blocks[i]);

	SCORECUSTOMLINKEDLIST_DELITEM(freeList_Level1Blocks,
				      convertedBlock->freeListItem);

	convertedBlock->type = SCORE_SEGMENTBLOCK_LEVEL1_UNAVAILABLE;
	
	SCORECUSTOMLINKEDLIST_APPEND(unavailableList_Level1Blocks,
				     convertedBlock,
				     convertedBlock->unavailableListItem);
      }
    }
  }
}


void ScoreSegmentTable::markCachedLevel0Block(ScoreSegmentBlock *block) {
  block->type = SCORE_SEGMENTBLOCK_LEVEL0_CACHED;

  SCORECUSTOMLINKEDLIST_DELITEM(usedList_Level0Blocks,
				block->usedListItem);

  SCORECUSTOMLINKEDLIST_APPEND(cachedList_Level0Blocks, block,
			       block->cachedListItem);
}


void ScoreSegmentTable::markCachedLevel1Block(ScoreSegmentBlock *block) {
  block->type = SCORE_SEGMENTBLOCK_LEVEL1_CACHED;

  SCORECUSTOMLINKEDLIST_DELITEM(usedList_Level1Blocks,
				block->usedListItem);

  SCORECUSTOMLINKEDLIST_APPEND(cachedList_Level1Blocks, block,
			       block->cachedListItem);
}


void ScoreSegmentTable::markUsedLevel0Block(ScoreSegmentBlock *block) {
  block->type = SCORE_SEGMENTBLOCK_LEVEL0_USED;

  SCORECUSTOMLINKEDLIST_DELITEM(cachedList_Level0Blocks,
				block->cachedListItem);

  SCORECUSTOMLINKEDLIST_APPEND(usedList_Level0Blocks, block,
			       block->usedListItem);
}


void ScoreSegmentTable::markUsedLevel1Block(ScoreSegmentBlock *block) {
  block->type = SCORE_SEGMENTBLOCK_LEVEL1_USED;

  SCORECUSTOMLINKEDLIST_DELITEM(cachedList_Level1Blocks,
				block->cachedListItem);

  SCORECUSTOMLINKEDLIST_APPEND(usedList_Level1Blocks, block,
			       block->usedListItem);
}


void ScoreSegmentTable::addToLoadBlockList(ScoreSegmentBlock *block, 
					   unsigned int blockOffset,
					   void *buffer, 
					   unsigned int bufferSize) {
  ScoreSegmentLoadDumpBlock *lBlock = &(loadBlocks[loadBlocks_count]);

  lBlock->owner = block->owner;
  lBlock->blockStart = block->start + blockOffset;
  lBlock->buffer = buffer;
  lBlock->bufferSize = bufferSize;
  lBlock->bufferSizeDependsOnOwnerAddrs = 0;

  loadBlocks_count++;
}


void ScoreSegmentTable::addToDumpBlockList(ScoreSegmentBlock *block,
					   unsigned int blockOffset,
					   void *buffer, 
					   unsigned int bufferSize) {
  ScoreSegmentLoadDumpBlock *dBlock = &(dumpBlocks[dumpBlocks_count]);

  dBlock->owner = block->owner;
  dBlock->blockStart = block->start + blockOffset;
  dBlock->buffer = buffer;
  dBlock->bufferSize = bufferSize;
  dBlock->bufferSizeDependsOnOwnerAddrs = 0;

  dumpBlocks_count++;
}


void ScoreSegmentTable::addToLoadBlockList_useOwnerAddrs(ScoreSegmentBlock *block, 
					                 unsigned int blockOffset,
					                 void *buffer) {
  ScoreSegmentLoadDumpBlock *lBlock = &(loadBlocks[loadBlocks_count]);

  lBlock->owner = block->owner;
  lBlock->blockStart = block->start + blockOffset;
  lBlock->buffer = buffer;
  lBlock->bufferSizeDependsOnOwnerAddrs = 1;

  loadBlocks_count++;
}


void ScoreSegmentTable::addToDumpBlockList_useOwnerAddrs(ScoreSegmentBlock *block,
					                 unsigned int blockOffset,
					                 void *buffer) {
  ScoreSegmentLoadDumpBlock *dBlock = &(dumpBlocks[dumpBlocks_count]);

  dBlock->owner = block->owner;
  dBlock->blockStart = block->start + blockOffset;
  dBlock->buffer = buffer;
  dBlock->bufferSizeDependsOnOwnerAddrs = 1;

  dumpBlocks_count++;
}


// required by LEDA for use with lists/etc.

// provides comparison operation between ScoreSegmentBlocks.
// NOTE: Right now, we only say 2 segment blocks are equal if their pointers
//       are equal. Otherwise, less than/greater than is determined
//       simply by their pointer values.
int compare(ScoreSegmentBlock * const & left, 
	    ScoreSegmentBlock * const & right) {
  if (left == right) {
    return(0);
  } else if (left < right) {
    return(-1);
  } else {
    return(1);
  }
}

