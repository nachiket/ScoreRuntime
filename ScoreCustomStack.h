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

#ifndef _ScoreCustomStack_H

#define _ScoreCustomStack_H

#if CHECK_CUSTOMBOUNDS
#include <assert.h>
#endif


template<class T>
class ScoreCustomStack {

public: 
  ScoreCustomStack(unsigned int numElems) {
    buffer = new T[numElems];
    count = 0;
#if CHECK_CUSTOMBOUNDS
    bound = numElems;
#endif
  }
  ~ScoreCustomStack() {
    delete(buffer);
  }

  T *buffer;
  unsigned int count;
#if CHECK_CUSTOMBOUNDS
  unsigned int bound;
#endif
};

#if CHECK_CUSTOMBOUNDS
#define SCORECUSTOMSTACK_PUSH(STACK, ELEM) \
  { \
    assert(STACK->count < STACK->bound); \
    STACK->buffer[STACK->count] = ELEM; \
    STACK->count++; \
  }
#else
#define SCORECUSTOMSTACK_PUSH(STACK, ELEM) \
  { \
    STACK->buffer[STACK->count] = ELEM; \
    STACK->count++; \
  }
#endif

#if CHECK_CUSTOMBOUNDS
#define SCORECUSTOMSTACK_POP(STACK, ELEM) \
  { \
    assert(STACK->count > 0); \
    STACK->count--; \
    ELEM = STACK->buffer[STACK->count]; \
  }
#else
#define SCORECUSTOMSTACK_POP(STACK, ELEM) \
  { \
    STACK->count--; \
    ELEM = STACK->buffer[STACK->count]; \
  }
#endif

#if CHECK_CUSTOMBOUNDS
#define SCORECUSTOMSTACK_POPONLY(STACK) \
  { \
    assert(STACK->count > 0); \
    STACK->count--; \
  }
#else
#define SCORECUSTOMSTACK_POPONLY(STACK) \
  { \
    STACK->count--; \
  }
#endif

#if CHECK_CUSTOMBOUNDS
#define SCORECUSTOMSTACK_FRONT(STACK, ELEM) \
  { \
    assert(STACK->count > 0); \
    ELEM = STACK->buffer[STACK->count - 1]; \
  }
#else
#define SCORECUSTOMSTACK_FRONT(STACK, ELEM) \
  { \
    ELEM = STACK->buffer[STACK->count - 1]; \
  }
#endif

#if CHECK_CUSTOMBOUNDS
#define SCORECUSTOMSTACK_ITEMAT(STACK, INDEX, ELEM) \
  { \
    assert((INDEX >= 0) && (INDEX < STACK->bound)); \
    ELEM = STACK->buffer[INDEX]; \
  }
#else
#define SCORECUSTOMSTACK_ITEMAT(STACK, INDEX, ELEM) \
  { \
    ELEM = STACK->buffer[INDEX]; \
  }
#endif

#define SCORECUSTOMSTACK_LENGTH(STACK) (STACK->count)

#define SCORECUSTOMSTACK_ISEMPTY(STACK) (STACK->count == 0)

#define SCORECUSTOMSTACK_CLEAR(STACK) \
  { \
    STACK->count = 0; \
  }

#endif
