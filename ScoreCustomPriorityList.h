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
// $Revision: 1.4 $
//
//////////////////////////////////////////////////////////////////////////////

#ifndef _ScoreCustomPriorityList_H

#define _ScoreCustomPriorityList_H

#include <values.h>
#include "ScoreCustomStack.h"

#if CHECK_CUSTOMBOUNDS
#include <assert.h>
#endif

#ifndef NULL
#define NULL 0
#endif

#define SCORECUSTOMPRIORITYLIST_NULL -1

#define SCORECUSTOMPRIORITYLISTITEM int

template<class T>
class ScoreCustomPriorityListItem {
 public:
  T theitem;
  int priority;
  SCORECUSTOMPRIORITYLISTITEM mapindex;
  SCORECUSTOMPRIORITYLISTITEM nodeindex;
#if CHECK_CUSTOMBOUNDS
  char valid;
#endif
};


template<class T>
class ScoreCustomPriorityList {

public: 
  ScoreCustomPriorityList(unsigned int numElems) {
    unsigned int i;

    buffer = new ScoreCustomPriorityListItem<T>[numElems+1];
    freeNodeStack = new ScoreCustomStack<unsigned int>(numElems);
    map = new ScoreCustomPriorityListItem<T>*[numElems+1];

    map[0] = &(buffer[0]);
#if CHECK_CUSTOMBOUNDS
    map[0]->valid = 0;
#endif
    map[0]->mapindex = 0;
    map[0]->nodeindex = 0;
    map[0]->priority = MAXINT;
    for (i = 1; i <= numElems; i++) {
      SCORECUSTOMSTACK_PUSH(freeNodeStack, ((numElems+1) - i));
      map[i] = NULL;
#if CHECK_CUSTOMBOUNDS
      buffer[i].valid = 0;
#endif
    }

    count = 0;
#if CHECK_CUSTOMBOUNDS
    bound = numElems;
#endif
  }
  ~ScoreCustomPriorityList() {
    delete(buffer);
    delete(freeNodeStack);
    delete(map);
  }

  ScoreCustomPriorityListItem<T> *buffer;

  ScoreCustomStack<unsigned int> *freeNodeStack;

  ScoreCustomPriorityListItem<T> **map;

  unsigned int count;
#if CHECK_CUSTOMBOUNDS
  unsigned int bound;
#endif
};

// This is meant for internal use!
#define SCORECUSTOMPRIORITYLIST_PARENTMAPINDEX(PRIORITYLISTMAPINDEX) (PRIORITYLISTMAPINDEX>>1)
#define SCORECUSTOMPRIORITYLIST_LEFTMAPINDEX(PRIORITYLISTMAPINDEX) (PRIORITYLISTMAPINDEX<<1)
#define SCORECUSTOMPRIORITYLIST_RIGHTMAPINDEX(PRIORITYLISTMAPINDEX) ((PRIORITYLISTMAPINDEX<<1)|0x1)

#if CHECK_CUSTOMBOUNDS
#define SCORECUSTOMPRIORITYLIST_INSERT(PRIORITYLIST, ELEM, PRIORITY, PRIORITYLISTITEM) \
  { \
    SCORECUSTOMPRIORITYLISTITEM freeNode; \
    SCORECUSTOMPRIORITYLISTITEM mapIndex, parentIndex; \
    assert(PRIORITYLIST->count < PRIORITYLIST->bound); \
    SCORECUSTOMSTACK_POP(PRIORITYLIST->freeNodeStack, freeNode); \
    PRIORITYLIST->buffer[freeNode].theitem = ELEM; \
    PRIORITYLIST->buffer[freeNode].priority = PRIORITY; \
    PRIORITYLIST->buffer[freeNode].valid = 1; \
    PRIORITYLIST->count++; \
    mapIndex = PRIORITYLIST->count; \
    if (PRIORITYLIST->count != 1) { \
      parentIndex = SCORECUSTOMPRIORITYLIST_PARENTMAPINDEX(mapIndex); \
      while (PRIORITY > PRIORITYLIST->map[parentIndex]->priority) { \
        PRIORITYLIST->map[parentIndex]->mapindex = mapIndex; \
        PRIORITYLIST->map[mapIndex] = PRIORITYLIST->map[parentIndex]; \
        mapIndex = parentIndex; \
        parentIndex = SCORECUSTOMPRIORITYLIST_PARENTMAPINDEX(mapIndex); \
      } \
    } \
    PRIORITYLIST->buffer[freeNode].mapindex = mapIndex; \
    PRIORITYLIST->buffer[freeNode].nodeindex = freeNode; \
    PRIORITYLIST->map[mapIndex] = &(PRIORITYLIST->buffer[freeNode]); \
    PRIORITYLISTITEM = freeNode; \
  }
#else
#define SCORECUSTOMPRIORITYLIST_INSERT(PRIORITYLIST, ELEM, PRIORITY, PRIORITYLISTITEM) \
  { \
    SCORECUSTOMPRIORITYLISTITEM freeNode; \
    SCORECUSTOMPRIORITYLISTITEM mapIndex, parentIndex; \
    SCORECUSTOMSTACK_POP(PRIORITYLIST->freeNodeStack, freeNode); \
    PRIORITYLIST->buffer[freeNode].theitem = ELEM; \
    PRIORITYLIST->buffer[freeNode].priority = PRIORITY; \
    PRIORITYLIST->count++; \
    mapIndex = PRIORITYLIST->count; \
    if (PRIORITYLIST->count != 1) { \
      parentIndex = SCORECUSTOMPRIORITYLIST_PARENTMAPINDEX(mapIndex); \
      while (PRIORITY > PRIORITYLIST->map[parentIndex]->priority) { \
        PRIORITYLIST->map[parentIndex]->mapindex = mapIndex; \
        PRIORITYLIST->map[mapIndex] = PRIORITYLIST->map[parentIndex]; \
        mapIndex = parentIndex; \
        parentIndex = SCORECUSTOMPRIORITYLIST_PARENTMAPINDEX(mapIndex); \
      } \
    } \
    PRIORITYLIST->buffer[freeNode].mapindex = mapIndex; \
    PRIORITYLIST->buffer[freeNode].nodeindex = freeNode; \
    PRIORITYLIST->map[mapIndex] = &(PRIORITYLIST->buffer[freeNode]); \
    PRIORITYLISTITEM = freeNode; \
  }
#endif

#if CHECK_CUSTOMBOUNDS
#define SCORECUSTOMPRIORITYLIST_POPMAX(PRIORITYLIST, ELEM) \
  { \
    SCORECUSTOMPRIORITYLISTITEM nodeIndex; \
    SCORECUSTOMPRIORITYLISTITEM floatMapIndex, floatLeftIndex, floatRightIndex; \
    SCORECUSTOMPRIORITYLISTITEM largestMapIndex; \
    SCORECUSTOMPRIORITYLISTITEM floatNodeIndex; \
    int floatPriority; \
    assert(PRIORITYLIST->count > 0); \
    nodeIndex = PRIORITYLIST->map[1]->nodeindex; \
    ELEM = PRIORITYLIST->buffer[nodeIndex].theitem; \
    PRIORITYLIST->buffer[nodeIndex].valid = 0; \
    floatMapIndex = PRIORITYLIST->count; \
    floatNodeIndex = PRIORITYLIST->map[floatMapIndex]->nodeindex; \
    PRIORITYLIST->count--; \
    floatMapIndex = 1; \
    while (1) { \
      floatPriority = PRIORITYLIST->buffer[floatNodeIndex].priority; \
      floatLeftIndex = SCORECUSTOMPRIORITYLIST_LEFTMAPINDEX(floatMapIndex); \
      floatRightIndex = SCORECUSTOMPRIORITYLIST_RIGHTMAPINDEX(floatMapIndex); \
      if ((floatLeftIndex <= (int) PRIORITYLIST->count) && \
          (PRIORITYLIST->map[floatLeftIndex]->priority > floatPriority)) { \
        largestMapIndex = floatLeftIndex; \
        floatPriority = PRIORITYLIST->map[floatLeftIndex]->priority; \
      } else { \
        largestMapIndex = floatMapIndex; \
      } \
      if ((floatRightIndex <= (int) PRIORITYLIST->count) && \
          (PRIORITYLIST->map[floatRightIndex]->priority > floatPriority)) { \
        largestMapIndex = floatRightIndex; \
      } \
      if (largestMapIndex != floatMapIndex) { \
        PRIORITYLIST->map[largestMapIndex]->mapindex = floatMapIndex; \
        PRIORITYLIST->map[floatMapIndex] = PRIORITYLIST->map[largestMapIndex]; \
        floatMapIndex = largestMapIndex; \
      } else { \
        break; \
      } \
    } \
    PRIORITYLIST->map[floatMapIndex] = &(PRIORITYLIST->buffer[floatNodeIndex]); \
    PRIORITYLIST->buffer[floatNodeIndex].mapindex = floatMapIndex; \
    SCORECUSTOMSTACK_PUSH(PRIORITYLIST->freeNodeStack, nodeIndex); \
  }
#else
#define SCORECUSTOMPRIORITYLIST_POPMAX(PRIORITYLIST, ELEM) \
  { \
    SCORECUSTOMPRIORITYLISTITEM nodeIndex; \
    SCORECUSTOMPRIORITYLISTITEM floatMapIndex, floatLeftIndex, floatRightIndex; \
    SCORECUSTOMPRIORITYLISTITEM largestMapIndex; \
    SCORECUSTOMPRIORITYLISTITEM floatNodeIndex; \
    int floatPriority; \
    nodeIndex = PRIORITYLIST->map[1]->nodeindex; \
    ELEM = PRIORITYLIST->buffer[nodeIndex].theitem; \
    floatMapIndex = PRIORITYLIST->count; \
    floatNodeIndex = PRIORITYLIST->map[floatMapIndex]->nodeindex; \
    PRIORITYLIST->count--; \
    floatMapIndex = 1; \
    while (1) { \
      floatPriority = PRIORITYLIST->buffer[floatNodeIndex].priority; \
      floatLeftIndex = SCORECUSTOMPRIORITYLIST_LEFTMAPINDEX(floatMapIndex); \
      floatRightIndex = SCORECUSTOMPRIORITYLIST_RIGHTMAPINDEX(floatMapIndex); \
      if ((floatLeftIndex <= (int) PRIORITYLIST->count) && \
          (PRIORITYLIST->map[floatLeftIndex]->priority > floatPriority)) { \
        largestMapIndex = floatLeftIndex; \
        floatPriority = PRIORITYLIST->map[floatLeftIndex]->priority; \
      } else { \
        largestMapIndex = floatMapIndex; \
      } \
      if ((floatRightIndex <= (int) PRIORITYLIST->count) && \
          (PRIORITYLIST->map[floatRightIndex]->priority > floatPriority)) { \
        largestMapIndex = floatRightIndex; \
      } \
      if (largestMapIndex != floatMapIndex) { \
        PRIORITYLIST->map[largestMapIndex]->mapindex = floatMapIndex; \
        PRIORITYLIST->map[floatMapIndex] = PRIORITYLIST->map[largestMapIndex]; \
        floatMapIndex = largestMapIndex; \
      } else { \
        break; \
      } \
    } \
    PRIORITYLIST->map[floatMapIndex] = &(PRIORITYLIST->buffer[floatNodeIndex]); \
    PRIORITYLIST->buffer[floatNodeIndex].mapindex = floatMapIndex; \
    SCORECUSTOMSTACK_PUSH(PRIORITYLIST->freeNodeStack, nodeIndex); \
  }
#endif

#define SCORECUSTOMPRIORITYLIST_LENGTH(PRIORITYLIST) (PRIORITYLIST->count)

#define SCORECUSTOMPRIORITYLIST_ISEMPTY(PRIORITYLIST) (PRIORITYLIST->count == 0)

#if CHECK_CUSTOMBOUNDS
#define SCORECUSTOMPRIORITYLIST_CLEAR(PRIORITYLIST) \
  { \
    unsigned int _i; \
    PRIORITYLIST->count = 0; \
    for (i = 1; i <= PRIORITYLIST->bound; i++) { \
      PRIORITYLIST->buffer[i].valid = 0; \
    } \
  }
#else
#define SCORECUSTOMPRIORITYLIST_CLEAR(PRIORITYLIST) \
  { \
    PRIORITYLIST->count = 0; \
  }
#endif

#if CHECK_CUSTOMBOUNDS
#define SCORECUSTOMPRIORITYLIST_DELITEM(PRIORITYLIST, PRIORITYLISTITEM) \
  { \
    SCORECUSTOMPRIORITYLISTITEM mapIndex, parentIndex; \
    SCORECUSTOMPRIORITYLISTITEM floatMapIndex, floatLeftIndex, floatRightIndex; \
    SCORECUSTOMPRIORITYLISTITEM largestMapIndex; \
    SCORECUSTOMPRIORITYLISTITEM floatNodeIndex; \
    int floatPriority; \
    assert((PRIORITYLISTITEM > 0) && \
           (PRIORITYLISTITEM <= (int) PRIORITYLIST->bound) && \
           PRIORITYLIST->buffer[PRIORITYLISTITEM].valid); \
    mapIndex = PRIORITYLIST->buffer[PRIORITYLISTITEM].mapindex; \
    if (PRIORITYLIST->count != 1) { \
      parentIndex = SCORECUSTOMPRIORITYLIST_PARENTMAPINDEX(mapIndex); \
      while (parentIndex != 0) { \
        PRIORITYLIST->map[parentIndex]->mapindex = mapIndex; \
        PRIORITYLIST->map[mapIndex] = PRIORITYLIST->map[parentIndex]; \
        mapIndex = parentIndex; \
        parentIndex = SCORECUSTOMPRIORITYLIST_PARENTMAPINDEX(mapIndex); \
      } \
    } \
    PRIORITYLIST->buffer[PRIORITYLISTITEM].valid = 0; \
    floatMapIndex = PRIORITYLIST->count; \
    floatNodeIndex = PRIORITYLIST->map[floatMapIndex]->nodeindex; \
    PRIORITYLIST->count--; \
    SCORECUSTOMSTACK_PUSH(PRIORITYLIST->freeNodeStack, PRIORITYLISTITEM); \
    PRIORITYLISTITEM = SCORECUSTOMPRIORITYLIST_NULL; \
    floatMapIndex = 1; \
    while (1) { \
      floatPriority = PRIORITYLIST->buffer[floatNodeIndex].priority; \
      floatLeftIndex = SCORECUSTOMPRIORITYLIST_LEFTMAPINDEX(floatMapIndex); \
      floatRightIndex = SCORECUSTOMPRIORITYLIST_RIGHTMAPINDEX(floatMapIndex); \
      if ((floatLeftIndex <= (int) PRIORITYLIST->count) && \
          (PRIORITYLIST->map[floatLeftIndex]->priority > floatPriority)) { \
        largestMapIndex = floatLeftIndex; \
        floatPriority = PRIORITYLIST->map[floatLeftIndex]->priority; \
      } else { \
        largestMapIndex = floatMapIndex; \
      } \
      if ((floatRightIndex <= (int) PRIORITYLIST->count) && \
          (PRIORITYLIST->map[floatRightIndex]->priority > floatPriority)) { \
        largestMapIndex = floatRightIndex; \
      } \
      if (largestMapIndex != floatMapIndex) { \
        PRIORITYLIST->map[largestMapIndex]->mapindex = floatMapIndex; \
        PRIORITYLIST->map[floatMapIndex] = PRIORITYLIST->map[largestMapIndex]; \
        floatMapIndex = largestMapIndex; \
      } else { \
        break; \
      } \
    } \
    PRIORITYLIST->map[floatMapIndex] = &(PRIORITYLIST->buffer[floatNodeIndex]); \
    PRIORITYLIST->buffer[floatNodeIndex].mapindex = floatMapIndex; \
  }
#else
#define SCORECUSTOMPRIORITYLIST_DELITEM(PRIORITYLIST, PRIORITYLISTITEM) \
  { \
    SCORECUSTOMPRIORITYLISTITEM mapIndex, parentIndex; \
    SCORECUSTOMPRIORITYLISTITEM floatMapIndex, floatLeftIndex, floatRightIndex; \
    SCORECUSTOMPRIORITYLISTITEM largestMapIndex; \
    SCORECUSTOMPRIORITYLISTITEM floatNodeIndex; \
    int floatPriority; \
    mapIndex = PRIORITYLIST->buffer[PRIORITYLISTITEM].mapindex; \
    if (PRIORITYLIST->count != 1) { \
      parentIndex = SCORECUSTOMPRIORITYLIST_PARENTMAPINDEX(mapIndex); \
      while (parentIndex != 0) { \
        PRIORITYLIST->map[parentIndex]->mapindex = mapIndex; \
        PRIORITYLIST->map[mapIndex] = PRIORITYLIST->map[parentIndex]; \
        mapIndex = parentIndex; \
        parentIndex = SCORECUSTOMPRIORITYLIST_PARENTMAPINDEX(mapIndex); \
      } \
    } \
    floatMapIndex = PRIORITYLIST->count; \
    floatNodeIndex = PRIORITYLIST->map[floatMapIndex]->nodeindex; \
    PRIORITYLIST->count--; \
    SCORECUSTOMSTACK_PUSH(PRIORITYLIST->freeNodeStack, PRIORITYLISTITEM); \
    PRIORITYLISTITEM = SCORECUSTOMPRIORITYLIST_NULL; \
    floatMapIndex = 1; \
    while (1) { \
      floatPriority = PRIORITYLIST->buffer[floatNodeIndex].priority; \
      floatLeftIndex = SCORECUSTOMPRIORITYLIST_LEFTMAPINDEX(floatMapIndex); \
      floatRightIndex = SCORECUSTOMPRIORITYLIST_RIGHTMAPINDEX(floatMapIndex); \
      if ((floatLeftIndex <= (int) PRIORITYLIST->count) && \
          (PRIORITYLIST->map[floatLeftIndex]->priority > floatPriority)) { \
        largestMapIndex = floatLeftIndex; \
        floatPriority = PRIORITYLIST->map[floatLeftIndex]->priority; \
      } else { \
        largestMapIndex = floatMapIndex; \
      } \
      if ((floatRightIndex <= (int) PRIORITYLIST->count) && \
          (PRIORITYLIST->map[floatRightIndex]->priority > floatPriority)) { \
        largestMapIndex = floatRightIndex; \
      } \
      if (largestMapIndex != floatMapIndex) { \
        PRIORITYLIST->map[largestMapIndex]->mapindex = floatMapIndex; \
        PRIORITYLIST->map[floatMapIndex] = PRIORITYLIST->map[largestMapIndex]; \
        floatMapIndex = largestMapIndex; \
      } else { \
        break; \
      } \
    } \
    PRIORITYLIST->map[floatMapIndex] = &(PRIORITYLIST->buffer[floatNodeIndex]); \
    PRIORITYLIST->buffer[floatNodeIndex].mapindex = floatMapIndex; \
  }
#endif

#if CHECK_CUSTOMBOUNDS
#define SCORECUSTOMPRIORITYLIST_INCREASEPRIORITY(PRIORITYLIST, PRIORITYLISTITEM, PRIORITY) \
  { \
    SCORECUSTOMPRIORITYLISTITEM mapIndex, parentIndex; \
    assert((PRIORITYLISTITEM > 0) && \
           (PRIORITYLISTITEM <= (int) PRIORITYLIST->bound) && \
           PRIORITYLIST->buffer[PRIORITYLISTITEM].valid); \
    assert(PRIORITY >= PRIORITYLIST->buffer[PRIORITYLISTITEM].priority); \
    mapIndex = PRIORITYLIST->buffer[PRIORITYLISTITEM].mapindex; \
    if (PRIORITYLIST->count != 1) { \
      parentIndex = SCORECUSTOMPRIORITYLIST_PARENTMAPINDEX(mapIndex); \
      while (PRIORITY > PRIORITYLIST->map[parentIndex]->priority) { \
        PRIORITYLIST->map[parentIndex]->mapindex = mapIndex; \
        PRIORITYLIST->map[mapIndex] = PRIORITYLIST->map[parentIndex]; \
        mapIndex = parentIndex; \
        parentIndex = SCORECUSTOMPRIORITYLIST_PARENTMAPINDEX(mapIndex); \
      } \
    } \
    PRIORITYLIST->buffer[PRIORITYLISTITEM].priority = PRIORITY; \
    PRIORITYLIST->buffer[PRIORITYLISTITEM].mapindex = mapIndex; \
    PRIORITYLIST->map[mapIndex] = &(PRIORITYLIST->buffer[PRIORITYLISTITEM]); \
  }
#else
#define SCORECUSTOMPRIORITYLIST_INCREASEPRIORITY(PRIORITYLIST, PRIORITYLISTITEM, PRIORITY) \
  { \
    SCORECUSTOMPRIORITYLISTITEM mapIndex, parentIndex; \
    mapIndex = PRIORITYLIST->buffer[PRIORITYLISTITEM].mapindex; \
    if (PRIORITYLIST->count != 1) { \
      parentIndex = SCORECUSTOMPRIORITYLIST_PARENTMAPINDEX(mapIndex); \
      while (PRIORITY > PRIORITYLIST->map[parentIndex]->priority) { \
        PRIORITYLIST->map[parentIndex]->mapindex = mapIndex; \
        PRIORITYLIST->map[mapIndex] = PRIORITYLIST->map[parentIndex]; \
        mapIndex = parentIndex; \
        parentIndex = SCORECUSTOMPRIORITYLIST_PARENTMAPINDEX(mapIndex); \
      } \
    } \
    PRIORITYLIST->buffer[PRIORITYLISTITEM].priority = PRIORITY; \
    PRIORITYLIST->buffer[PRIORITYLISTITEM].mapindex = mapIndex; \
    PRIORITYLIST->map[mapIndex] = &(PRIORITYLIST->buffer[PRIORITYLISTITEM]); \
  }
#endif

#if CHECK_CUSTOMBOUNDS
#define SCORECUSTOMPRIORITYLIST_ITEMATMAPINDEX(PRIORITYLIST, MAPINDEX, ELEM) \
  { \
    assert((MAPINDEX > 0) && (MAPINDEX <= PRIORITYLIST->count)); \
    ELEM = PRIORITYLIST->map[MAPINDEX]->theitem; \
  }
#else
#define SCORECUSTOMPRIORITYLIST_ITEMATMAPINDEX(PRIORITYLIST, MAPINDEX, ELEM) \
  { \
    ELEM = PRIORITYLIST->map[MAPINDEX]->theitem; \
  }
#endif

#endif
