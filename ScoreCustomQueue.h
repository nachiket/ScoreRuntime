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
// $Revision: 1.2 $
//
//////////////////////////////////////////////////////////////////////////////

#ifndef _ScoreCustomQueue_H

#define _ScoreCustomQueue_H

#if CHECK_CUSTOMBOUNDS
#include <assert.h>
#endif


template<class T>
class ScoreCustomQueue {

public: 
  ScoreCustomQueue(unsigned int numElems) {
    buffer = new T[numElems];
    head = 0;
    tail = 0;
    count = 0;
    bound = numElems;
  }
  ~ScoreCustomQueue() {
    delete(buffer);
  }

  T *buffer;
  unsigned int head, tail, count, bound;
};

#if CHECK_CUSTOMBOUNDS
#define SCORECUSTOMQUEUE_QUEUE(QUEUE, ELEM) \
  { \
    assert(!((QUEUE->tail == (QUEUE->head - 1)) || \
             ((QUEUE->head == 0) && (QUEUE->tail == (QUEUE->bound - 1))))); \
    QUEUE->buffer[QUEUE->tail] = ELEM; \
    QUEUE->tail++; \
    QUEUE->count++; \
    if (QUEUE->tail == QUEUE->bound) { \
      QUEUE->tail = 0; \
    } \
  }
#else
#define SCORECUSTOMQUEUE_QUEUE(QUEUE, ELEM) \
  { \
    QUEUE->buffer[QUEUE->tail] = ELEM; \
    QUEUE->tail++; \
    QUEUE->count++; \
    if (QUEUE->tail == QUEUE->bound) { \
      QUEUE->tail = 0; \
    } \
  }
#endif

#if CHECK_CUSTOMBOUNDS
#define SCORECUSTOMQUEUE_DEQUEUE(QUEUE, ELEM) \
  { \
    assert(QUEUE->head != QUEUE->tail); \
    ELEM = QUEUE->buffer[QUEUE->head]; \
    QUEUE->head++; \
    QUEUE->count--; \
    if (QUEUE->head == QUEUE->bound) { \
      QUEUE->head = 0; \
    } \
  }
#else
#define SCORECUSTOMQUEUE_DEQUEUE(QUEUE, ELEM) \
  { \
    ELEM = QUEUE->buffer[QUEUE->head]; \
    QUEUE->head++; \
    QUEUE->count--; \
    if (QUEUE->head == QUEUE->bound) { \
      QUEUE->head = 0; \
    } \
  }
#endif

#define SCORECUSTOMQUEUE_LENGTH(QUEUE) (QUEUE->count)

#define SCORECUSTOMQUEUE_ISEMPTY(QUEUE) (QUEUE->count == 0)

#define SCORECUSTOMQUEUE_CLEAR(QUEUE) \
  { \
    QUEUE->head = 0; \
    QUEUE->tail = 0; \
    QUEUE->count = 0; \
  }

#endif
