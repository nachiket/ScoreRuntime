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

#ifndef _ScoreCustomList_H

#define _ScoreCustomList_H

#if CHECK_CUSTOMBOUNDS
#include <assert.h>
#endif


template<class T>
class ScoreCustomList {

public: 
  ScoreCustomList(unsigned int numElems) {
    buffer = new T[numElems];
    count = 0;
#if CHECK_CUSTOMBOUNDS
    bound = numElems;
#endif
  }
  ~ScoreCustomList() {
    delete(buffer);
  }

  T *buffer;
  unsigned int count;
#if CHECK_CUSTOMBOUNDS
  unsigned int bound;
#endif
};

#if CHECK_CUSTOMBOUNDS
#define SCORECUSTOMLIST_APPEND(LIST, ELEM) \
  { \
    assert(LIST->count < LIST->bound); \
    LIST->buffer[LIST->count] = ELEM; \
    LIST->count++; \
  }
#else
#define SCORECUSTOMLIST_APPEND(LIST, ELEM) \
  { \
    LIST->buffer[LIST->count] = ELEM; \
    LIST->count++; \
  }
#endif

#define SCORECUSTOMLIST_REMOVE(LIST, ELEM) \
  { \
    unsigned int __i; \
  \
    for (__i = 0; __i < LIST->count; __i++) { \
      if (LIST->buffer[__i] == ELEM) { \
        LIST->count--; \
        LIST->buffer[__i] = LIST->buffer[LIST->count]; \
        break; \
      } \
    } \
  }

#define SCORECUSTOMLIST_REPLACE(LIST, OLDELEM, NEWELEM) \
  { \
    unsigned int __i; \
  \
    for (__i = 0; __i < LIST->count; __i++) { \
      if (LIST->buffer[__i] == OLDELEM) { \
        LIST->buffer[__i] = NEWELEM; \
        break; \
      } \
    } \
  }

// NOTE: USAGE OF THIS WILL CAUSE THIS MACRO WILL DISRUPT THE INDEX ORDER OF
//       THE LIST! THE LAST ELEMENT IN THE LIST WILL BE MOVED TO FILL THE
//       CREATED SPACE! THIS MEANS THAT THE INDEX SHOULD NOT BE ADVANCED TO
//       LOOK FOR THE NEXT ELEMENT! JUST CONTINUE READING THIS ONE!
#if CHECK_CUSTOMBOUNDS
#define SCORECUSTOMLIST_REMOVEITEMAT(LIST, INDEX) \
  { \
    assert((INDEX >= 0) && (INDEX < LIST->bound)); \
    LIST->count--; \
    LIST->buffer[INDEX] = LIST->buffer[LIST->count]; \
  }
#else
#define SCORECUSTOMLIST_REMOVEITEMAT(LIST, INDEX) \
  { \
    LIST->count--; \
    LIST->buffer[INDEX] = LIST->buffer[LIST->count]; \
  }
#endif

#define SCORECUSTOMLIST_LENGTH(LIST) (LIST->count)

#define SCORECUSTOMLIST_ISEMPTY(LIST) (LIST->count == 0)

#define SCORECUSTOMLIST_CLEAR(LIST) \
  { \
    LIST->count = 0; \
  }

#if CHECK_CUSTOMBOUNDS
#define SCORECUSTOMLIST_ITEMAT(LIST, INDEX, ELEM) \
  { \
    assert((INDEX >= 0) && (INDEX < LIST->bound)); \
    ELEM = LIST->buffer[INDEX]; \
  }
#else
#define SCORECUSTOMLIST_ITEMAT(LIST, INDEX, ELEM) \
  { \
    ELEM = LIST->buffer[INDEX]; \
  }
#endif

#if CHECK_CUSTOMBOUNDS
#define SCORECUSTOMLIST_ASSIGN(LIST, INDEX, ELEM) \
  { \
    assert((INDEX >= 0) && (INDEX < LIST->bound)); \
    LIST->buffer[INDEX] = ELEM; \
  }
#else
#define SCORECUSTOMLIST_ASSIGN(LIST, INDEX, ELEM) \
  { \
    LIST->buffer[INDEX] = ELEM; \
  }
#endif

#endif
