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
// $Revision: 1.12 $
//
//////////////////////////////////////////////////////////////////////////////
#ifndef _ScoreSegmentTable_H

#define _ScoreSegmentTable_H

#include "ScoreCustomLinkedList.h"


// prototypes to avoid circular includes.
class ScoreGraphNode;


// defines the types of data stored in the segment block.
// FREE means that this segment block is can be allocated immediately.
// USED means that this segment block is currently occupied and the
//    contents must first be flushed before allocation.
// UNAVAILABLE means that this segment block is currently unavailable because
//    its space is used by another level (i.e. LEVEL0 being used by LEVEL1,
//    or viseversa). In order to use this segment block, the other level's
//    block must be flushed and then the space reclaimed by the current
//    level.
// CRUFT means that this segment block at the current level is too small to
//    be used for a segment block. This does not mean that this space is
//    unusable in the other level, just that it is unusable in the current
//    level.
#define SCORE_SEGMENTBLOCK_LEVEL0_FREE         0
#define SCORE_SEGMENTBLOCK_LEVEL1_FREE         1
#define SCORE_SEGMENTBLOCK_LEVEL0_USED         2
#define SCORE_SEGMENTBLOCK_LEVEL1_USED         3
#define SCORE_SEGMENTBLOCK_LEVEL0_UNAVAILABLE  4
#define SCORE_SEGMENTBLOCK_LEVEL1_UNAVAILABLE  5
#define SCORE_SEGMENTBLOCK_LEVEL0_CRUFT        6
#define SCORE_SEGMENTBLOCK_LEVEL1_CRUFT        7
#define SCORE_SEGMENTBLOCK_LEVEL0_CACHED       8
#define SCORE_SEGMENTBLOCK_LEVEL1_CACHED       9

class ScoreSegmentTable;

// ScoreSegmentBlock: this represents a contiguous block of memory in a
//   physical memory segment.
class ScoreSegmentBlock {
 public:
  ScoreSegmentBlock();
  ~ScoreSegmentBlock();


  // indicates the type of data stores in this segment block.
  unsigned int type;

  // indicates the start index of the segment block.
  unsigned int start;

  // indicates the size of the segment block.
  unsigned int size;

  // pointer to the page or memory segment who owns the data stored in this
  // segment block.
  ScoreGraphNode *owner;

  // points to the segment table the block belongs to.
  ScoreSegmentTable *parentTable;

  // points to the segment block the block belongs to (LEVEL1->LEVEL0).
  unsigned int parentBlockIndex;

  // points to the start subblock and the size of the range of subblocks
  // that belong to the block
  // (LEVEL0->LEVEL1).
  unsigned int childBlockStartIndex, childBlockSize;

  // keeps track of the number of child subblocks that are currently in
  // use.
  unsigned int childBlocksInUse;

  SCORECUSTOMLINKEDLISTITEM freeListItem, unavailableListItem, 
    usedListItem, cachedListItem;

 private:
};


// ScoreSegmentLoadDumpBlock: used to keep track of blocks to load/dump.
class ScoreSegmentLoadDumpBlock {
 public:
  ScoreSegmentLoadDumpBlock();
  ~ScoreSegmentLoadDumpBlock();


  // pointer to the page or memory segment who owns the data stored in this
  // segment block.
  ScoreGraphNode *owner;

  // indicates the start index of the segment block (offset already included).
  unsigned int blockStart;

  // indicates the buffer.
  void *buffer;

  // indicates the size of the buffer.
  unsigned int bufferSize;

  // indicates if the size of the buffer should be determined by looking at
  // the owner node. (this is used primarily for stitch buffer data once
  // the array has been stopped so that valid data tokens are loaded/dumped).
  char bufferSizeDependsOnOwnerAddrs;

 private:
};


// ScoreSegmentTable: this keeps track of ScoreSegmentBlocks in
//   physical memory segments.
class ScoreSegmentTable {
 public:
  ScoreSegmentTable(unsigned int newLoc, unsigned int newCMBSize);
  ~ScoreSegmentTable();
  ScoreSegmentBlock *allocateLevel0Block(ScoreGraphNode *owner);
  ScoreSegmentBlock *allocateLevel1Block(ScoreGraphNode *owner);
  void freeCachedLevel0Block(ScoreSegmentBlock *block);
  void freeCachedLevel1Block(ScoreSegmentBlock *block);
  void freeCachedLevel1Block_nomerge(ScoreSegmentBlock *block);
  void freeUsedLevel0Block(ScoreSegmentBlock *block);
  void freeUsedLevel1Block(ScoreSegmentBlock *block);
  void markCachedLevel0Block(ScoreSegmentBlock *block);
  void markCachedLevel1Block(ScoreSegmentBlock *block);
  void markUsedLevel0Block(ScoreSegmentBlock *block);
  void markUsedLevel1Block(ScoreSegmentBlock *block);
  void addToLoadBlockList(ScoreSegmentBlock *block, 
			  unsigned int blockOffset,
			  void *buffer, unsigned int bufferSize);
  void addToDumpBlockList(ScoreSegmentBlock *block,
			  unsigned int blockOffset,
			  void *buffer, unsigned int bufferSize);
  void addToLoadBlockList_useOwnerAddrs(ScoreSegmentBlock *block, 
			                unsigned int blockOffset,
			                void *buffer);
  void addToDumpBlockList_useOwnerAddrs(ScoreSegmentBlock *block,
			                unsigned int blockOffset,
			                void *buffer);


  // stores the location of the physical memory segment this ScoreSegmentTable
  // is responsible for.
  unsigned int loc;

  // stores the size of the CMB.
  unsigned int cmbSize;

  // the level0 and level1 segment block arrays.
  ScoreSegmentBlock *level0Blocks;
  ScoreSegmentBlock *level1Blocks;

  // the load/dump list for level0 and level1 segment blocks.
  ScoreSegmentLoadDumpBlock *dumpBlocks;
  ScoreSegmentLoadDumpBlock *loadBlocks;

  // marks how many blocks are in the load/dump list and the capacity.
  unsigned int loadBlocks_count, loadBlocks_capacity;
  unsigned int dumpBlocks_count, dumpBlocks_capacity;

  // lists for managing the various segment blocks.
  ScoreCustomLinkedList<ScoreSegmentBlock *> *freeList_Level0Blocks;
  ScoreCustomLinkedList<ScoreSegmentBlock *> *freeList_Level1Blocks;
  ScoreCustomLinkedList<ScoreSegmentBlock *> *unavailableList_Level0Blocks;
  ScoreCustomLinkedList<ScoreSegmentBlock *> *unavailableList_Level1Blocks;
  ScoreCustomLinkedList<ScoreSegmentBlock *> *usedList_Level0Blocks;
  ScoreCustomLinkedList<ScoreSegmentBlock *> *usedList_Level1Blocks;
  ScoreCustomLinkedList<ScoreSegmentBlock *> *cachedList_Level0Blocks;
  ScoreCustomLinkedList<ScoreSegmentBlock *> *cachedList_Level1Blocks;

 private:
};

// needed by LEDA for use with lists/etc.
int compare(ScoreSegmentBlock * const & left, 
            ScoreSegmentBlock * const & right);

#endif


