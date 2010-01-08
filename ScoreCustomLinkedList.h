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
// $Revision: 1.7 $
//
//////////////////////////////////////////////////////////////////////////////

#ifndef _ScoreCustomLinkedList_H

#define _ScoreCustomLinkedList_H

#include "ScoreCustomStack.h"

#if CHECK_CUSTOMBOUNDS
#include <assert.h>
#endif

#define SCORECUSTOMLINKEDLIST_NULL -1

#define SCORECUSTOMLINKEDLISTITEM int

template<class T>
class ScoreCustomLinkedListItem {
 public:
  T theitem;
  SCORECUSTOMLINKEDLISTITEM prev, next;
#if CHECK_CUSTOMBOUNDS
  char valid;
#endif
};


template<class T>
class ScoreCustomLinkedList {

public: 
  ScoreCustomLinkedList(unsigned int numElems) {
    unsigned int i;

    buffer = new ScoreCustomLinkedListItem<T>[numElems];
    freeNodeStack = new ScoreCustomStack<unsigned int>(numElems);

    for (i = 0; i < numElems; i++) {
      SCORECUSTOMSTACK_PUSH(freeNodeStack, (numElems - (i+1)));
#if CHECK_CUSTOMBOUNDS
      buffer[i].valid = 0;
#endif
    }

    head = SCORECUSTOMLINKEDLIST_NULL;
    tail = SCORECUSTOMLINKEDLIST_NULL;

    count = 0;
#if CHECK_CUSTOMBOUNDS
    bound = numElems;
#endif
  }
  ~ScoreCustomLinkedList() {
    delete(buffer);
    delete(freeNodeStack);
  }

  ScoreCustomLinkedListItem<T> *buffer;

  ScoreCustomStack<unsigned int> *freeNodeStack;

  SCORECUSTOMLINKEDLISTITEM head, tail;

  unsigned int count;
#if CHECK_CUSTOMBOUNDS
  unsigned int bound;
#endif
};

#if CHECK_CUSTOMBOUNDS
#define SCORECUSTOMLINKEDLIST_PREPEND(LINKEDLIST, ELEM, LINKEDLISTITEM) \
  { \
    SCORECUSTOMLINKEDLISTITEM freeNode; \
    assert(LINKEDLIST->count < LINKEDLIST->bound); \
    SCORECUSTOMSTACK_POP(LINKEDLIST->freeNodeStack, freeNode); \
    if (LINKEDLIST->head != SCORECUSTOMLINKEDLIST_NULL) { \
      LINKEDLIST->buffer[freeNode].next = LINKEDLIST->head; \
    } \
    if (LINKEDLIST->tail == SCORECUSTOMLINKEDLIST_NULL) { \
      LINKEDLIST->tail = freeNode; \
    } \
    LINKEDLIST->buffer[freeNode].prev = SCORECUSTOMLINKEDLIST_NULL; \
    LINKEDLIST->buffer[freeNode].next = LINKEDLIST->head; \
    LINKEDLIST->buffer[freeNode].theitem = ELEM; \
    LINKEDLIST->buffer[freeNode].valid = 1; \
    LINKEDLIST->head = freeNode; \
    LINKEDLIST->count++; \
    LINKEDLISTITEM = freeNode; \
  }
#else
#define SCORECUSTOMLINKEDLIST_PREPEND(LINKEDLIST, ELEM, LINKEDLISTITEM) \
  { \
    SCORECUSTOMLINKEDLISTITEM freeNode; \
    SCORECUSTOMSTACK_POP(LINKEDLIST->freeNodeStack, freeNode); \
    if (LINKEDLIST->head != SCORECUSTOMLINKEDLIST_NULL) { \
      LINKEDLIST->buffer[freeNode].next = LINKEDLIST->head; \
    } \
    if (LINKEDLIST->tail == SCORECUSTOMLINKEDLIST_NULL) { \
      LINKEDLIST->tail = freeNode; \
    } \
    LINKEDLIST->buffer[freeNode].prev = SCORECUSTOMLINKEDLIST_NULL; \
    LINKEDLIST->buffer[freeNode].next = LINKEDLIST->head; \
    LINKEDLIST->buffer[freeNode].theitem = ELEM; \
    LINKEDLIST->head = freeNode; \
    LINKEDLIST->count++; \
    LINKEDLISTITEM = freeNode; \
  }
#endif

#if CHECK_CUSTOMBOUNDS
#define SCORECUSTOMLINKEDLIST_APPEND(LINKEDLIST, ELEM, LINKEDLISTITEM) \
  { \
    SCORECUSTOMLINKEDLISTITEM freeNode; \
    assert(LINKEDLIST->count < LINKEDLIST->bound); \
    SCORECUSTOMSTACK_POP(LINKEDLIST->freeNodeStack, freeNode); \
    if (LINKEDLIST->head == SCORECUSTOMLINKEDLIST_NULL) { \
      LINKEDLIST->head = freeNode; \
    } \
    if (LINKEDLIST->tail != SCORECUSTOMLINKEDLIST_NULL) { \
      LINKEDLIST->buffer[LINKEDLIST->tail].next = freeNode; \
    } \
    LINKEDLIST->buffer[freeNode].prev = LINKEDLIST->tail; \
    LINKEDLIST->buffer[freeNode].next = SCORECUSTOMLINKEDLIST_NULL; \
    LINKEDLIST->buffer[freeNode].theitem = ELEM; \
    LINKEDLIST->buffer[freeNode].valid = 1; \
    LINKEDLIST->tail = freeNode; \
    LINKEDLIST->count++; \
    LINKEDLISTITEM = freeNode; \
    if (CUSTOM_VERBOSE) cerr << "APPEND: list = " << ((long)(LINKEDLIST)) << ", elem = " << ((long)(ELEM)) << \
       ", item = " << LINKEDLISTITEM << endl; \
  }
#else
#define SCORECUSTOMLINKEDLIST_APPEND(LINKEDLIST, ELEM, LINKEDLISTITEM) \
  { \
    SCORECUSTOMLINKEDLISTITEM freeNode; \
    SCORECUSTOMSTACK_POP(LINKEDLIST->freeNodeStack, freeNode); \
    if (LINKEDLIST->head == SCORECUSTOMLINKEDLIST_NULL) { \
      LINKEDLIST->head = freeNode; \
    } \
    if (LINKEDLIST->tail != SCORECUSTOMLINKEDLIST_NULL) { \
      LINKEDLIST->buffer[LINKEDLIST->tail].next = freeNode; \
    } \
    LINKEDLIST->buffer[freeNode].prev = LINKEDLIST->tail; \
    LINKEDLIST->buffer[freeNode].next = SCORECUSTOMLINKEDLIST_NULL; \
    LINKEDLIST->buffer[freeNode].theitem = ELEM; \
    LINKEDLIST->tail = freeNode; \
    LINKEDLIST->count++; \
    LINKEDLISTITEM = freeNode; \
  }
#endif

#if CHECK_CUSTOMBOUNDS
#define SCORECUSTOMLINKEDLIST_POP(LINKEDLIST, ELEM) \
  { \
    SCORECUSTOMLINKEDLISTITEM freeNode; \
    assert(LINKEDLIST->count > 0); \
    freeNode = LINKEDLIST->head; \
    ELEM = LINKEDLIST->buffer[freeNode].theitem; \
    LINKEDLIST->head = LINKEDLIST->buffer[freeNode].next; \
    if (LINKEDLIST->head == SCORECUSTOMLINKEDLIST_NULL) { \
      LINKEDLIST->tail = SCORECUSTOMLINKEDLIST_NULL; \
    } else { \
      LINKEDLIST->buffer[LINKEDLIST->head].prev = SCORECUSTOMLINKEDLIST_NULL; \
    } \
    LINKEDLIST->buffer[freeNode].valid = 0; \
    LINKEDLIST->count--; \
    SCORECUSTOMSTACK_PUSH(LINKEDLIST->freeNodeStack, freeNode); \
  }
#else
#define SCORECUSTOMLINKEDLIST_POP(LINKEDLIST, ELEM) \
  { \
    SCORECUSTOMLINKEDLISTITEM freeNode; \
    freeNode = LINKEDLIST->head; \
    ELEM = LINKEDLIST->buffer[freeNode].theitem; \
    LINKEDLIST->head = LINKEDLIST->buffer[freeNode].next; \
    if (LINKEDLIST->head == SCORECUSTOMLINKEDLIST_NULL) { \
      LINKEDLIST->tail = SCORECUSTOMLINKEDLIST_NULL; \
    } else { \
      LINKEDLIST->buffer[LINKEDLIST->head].prev = SCORECUSTOMLINKEDLIST_NULL; \
    } \
    LINKEDLIST->count--; \
    SCORECUSTOMSTACK_PUSH(LINKEDLIST->freeNodeStack, freeNode); \
  }
#endif

#define SCORECUSTOMLINKEDLIST_LENGTH(LINKEDLIST) (LINKEDLIST->count)

#define SCORECUSTOMLINKEDLIST_ISEMPTY(LINKEDLIST) (LINKEDLIST->count == 0)

#if CHECK_CUSTOMBOUNDS
#define SCORECUSTOMLINKEDLIST_CLEAR(LINKEDLIST) \
  { \
    for (SCORECUSTOMLINKEDLISTITEM _it = LINKEDLIST->head; \
         _it != SCORECUSTOMLINKEDLIST_NULL; \
         _it = LINKEDLIST->buffer[_it].next){\
       SCORECUSTOMSTACK_PUSH(LINKEDLIST->freeNodeStack, _it); \
       LINKEDLIST->buffer[_it].valid = 0; \
    } \
    LINKEDLIST->count = 0; \
    LINKEDLIST->head = SCORECUSTOMLINKEDLIST_NULL; \
    LINKEDLIST->tail = SCORECUSTOMLINKEDLIST_NULL; \
    if (CUSTOM_VERBOSE) cerr << "CLEAR: list = " << ((long)(LINKEDLIST)) << endl; \
  }
#else
#define SCORECUSTOMLINKEDLIST_CLEAR(LINKEDLIST) \
  { \
    for (SCORECUSTOMLINKEDLISTITEM _it = LINKEDLIST->head; \
         _it != SCORECUSTOMLINKEDLIST_NULL; \
         _it = LINKEDLIST->buffer[_it].next){\
       SCORECUSTOMSTACK_PUSH(LINKEDLIST->freeNodeStack, _it); \
    } \
    LINKEDLIST->count = 0; \
    LINKEDLIST->head = SCORECUSTOMLINKEDLIST_NULL; \
    LINKEDLIST->tail = SCORECUSTOMLINKEDLIST_NULL; \
  }
#endif

#if CHECK_CUSTOMBOUNDS
#define SCORECUSTOMLINKEDLIST_DELITEM(LINKEDLIST, LINKEDLISTITEM) \
  { \
    SCORECUSTOMLINKEDLISTITEM _prev, _next; \
    assert((LINKEDLISTITEM >= 0) && \
           (LINKEDLISTITEM < (int) LINKEDLIST->bound) && \
           LINKEDLIST->buffer[LINKEDLISTITEM].valid); \
    if (CUSTOM_VERBOSE) cerr << "DELITEM: list = " << ((long)(LINKEDLIST)) << ", elem = " << ((long)(LINKEDLIST->buffer[LINKEDLISTITEM].theitem)) << ", item = " << LINKEDLISTITEM << endl; \
    _prev = LINKEDLIST->buffer[LINKEDLISTITEM].prev; \
    _next = LINKEDLIST->buffer[LINKEDLISTITEM].next; \
    if (_prev != SCORECUSTOMLINKEDLIST_NULL) { \
      LINKEDLIST->buffer[_prev].next = _next; \
    } else { \
      LINKEDLIST->head = _next; \
    } \
    if (_next != SCORECUSTOMLINKEDLIST_NULL) { \
      LINKEDLIST->buffer[_next].prev = _prev; \
    } else { \
      LINKEDLIST->tail = _prev; \
    } \
    LINKEDLIST->buffer[LINKEDLISTITEM].valid = 0; \
    LINKEDLIST->count--; \
    SCORECUSTOMSTACK_PUSH(LINKEDLIST->freeNodeStack, LINKEDLISTITEM); \
    LINKEDLISTITEM = SCORECUSTOMLINKEDLIST_NULL; \
  }
#else
#define SCORECUSTOMLINKEDLIST_DELITEM(LINKEDLIST, LINKEDLISTITEM) \
  { \
    SCORECUSTOMLINKEDLISTITEM _prev, _next; \
    _prev = LINKEDLIST->buffer[LINKEDLISTITEM].prev; \
    _next = LINKEDLIST->buffer[LINKEDLISTITEM].next; \
    if (_prev != SCORECUSTOMLINKEDLIST_NULL) { \
      LINKEDLIST->buffer[_prev].next = _next; \
    } else { \
      LINKEDLIST->head = _next; \
    } \
    if (_next != SCORECUSTOMLINKEDLIST_NULL) { \
      LINKEDLIST->buffer[_next].prev = _prev; \
    } else { \
      LINKEDLIST->tail = _prev; \
    } \
    LINKEDLIST->count--; \
    SCORECUSTOMSTACK_PUSH(LINKEDLIST->freeNodeStack, LINKEDLISTITEM); \
    LINKEDLISTITEM = SCORECUSTOMLINKEDLIST_NULL; \
  }
#endif

#if CHECK_CUSTOMBOUNDS
#define SCORECUSTOMLINKEDLIST_DELITEM_GOTONEXT(LINKEDLIST, LINKEDLISTITEM) \
  { \
    SCORECUSTOMLINKEDLISTITEM _prev, _next; \
    assert((LINKEDLISTITEM >= 0) && \
           (LINKEDLISTITEM < (int) LINKEDLIST->bound) && \
           LINKEDLIST->buffer[LINKEDLISTITEM].valid); \
    if (CUSTOM_VERBOSE) cerr << "DELITEM: list = " << ((unsigned int)(LINKEDLIST)) << ", elem = " << ((unsigned int)(LINKEDLIST->buffer[LINKEDLISTITEM].theitem) << ", item = " << LINKEDLISTITEM << endl; \
    _prev = LINKEDLIST->buffer[LINKEDLISTITEM].prev; \
    _next = LINKEDLIST->buffer[LINKEDLISTITEM].next; \
    if (_prev != SCORECUSTOMLINKEDLIST_NULL) { \
      LINKEDLIST->buffer[_prev].next = _next; \
    } else { \
      LINKEDLIST->head = _next; \
    } \
    if (_next != SCORECUSTOMLINKEDLIST_NULL) { \
      LINKEDLIST->buffer[_next].prev = _prev; \
    } else { \
      LINKEDLIST->tail = _prev; \
    } \
    LINKEDLIST->buffer[LINKEDLISTITEM].valid = 0; \
    LINKEDLIST->count--; \
    SCORECUSTOMSTACK_PUSH(LINKEDLIST->freeNodeStack, LINKEDLISTITEM); \
    LINKEDLISTITEM = _next; \
  }
#else
#define SCORECUSTOMLINKEDLIST_DELITEM_GOTONEXT(LINKEDLIST, LINKEDLISTITEM) \
  { \
    SCORECUSTOMLINKEDLISTITEM _prev, _next; \
    _prev = LINKEDLIST->buffer[LINKEDLISTITEM].prev; \
    _next = LINKEDLIST->buffer[LINKEDLISTITEM].next; \
    if (_prev != SCORECUSTOMLINKEDLIST_NULL) { \
      LINKEDLIST->buffer[_prev].next = _next; \
    } else { \
      LINKEDLIST->head = _next; \
    } \
    if (_next != SCORECUSTOMLINKEDLIST_NULL) { \
      LINKEDLIST->buffer[_next].prev = _prev; \
    } else { \
      LINKEDLIST->tail = _prev; \
    } \
    LINKEDLIST->count--; \
    SCORECUSTOMSTACK_PUSH(LINKEDLIST->freeNodeStack, LINKEDLISTITEM); \
    LINKEDLISTITEM = _next; \
  }
#endif


#define SCORECUSTOMLINKEDLIST_HEAD(LINKEDLIST, LINKEDLISTITEM) \
  { \
    LINKEDLISTITEM = LINKEDLIST->head; \
  }

#define SCORECUSTOMLINKEDLIST_TAIL(LINKEDLIST, LINKEDLISTITEM) \
  { \
    LINKEDLISTITEM = LINKEDLIST->tail; \
  }

#if CHECK_CUSTOMBOUND
#define SCORECUSTOMLINKEDLIST_GOTONEXT(LINKEDLIST, LINKEDLISTITEM) \
  { \
    assert((LINKEDLISTITEM >= 0) && \
           (LINKEDLISTITEM < (int) LINKEDLIST->bound) && \
           LINKEDLIST->buffer[LINKEDLISTITEM].valid); \
    LINKEDLISTITEM = LINKEDLIST->buffer[LINKEDLISTITEM].next; \
  }
#else
#define SCORECUSTOMLINKEDLIST_GOTONEXT(LINKEDLIST, LINKEDLISTITEM) \
  { \
    LINKEDLISTITEM = LINKEDLIST->buffer[LINKEDLISTITEM].next; \
  }
#endif

#if CHECK_CUSTOMBOUNDS
#define SCORECUSTOMLINKEDLIST_ITEMAT(LINKEDLIST, LINKEDLISTITEM, ELEM) \
  { \
    assert((LINKEDLISTITEM >= 0) && \
	   (LINKEDLISTITEM < (int) LINKEDLIST->bound) && \
	   (LINKEDLIST->buffer[LINKEDLISTITEM].valid)); \
    ELEM = LINKEDLIST->buffer[LINKEDLISTITEM].theitem; \
  }
#else
#define SCORECUSTOMLINKEDLIST_ITEMAT(LINKEDLIST, LINKEDLISTITEM, ELEM) \
  { \
    ELEM = LINKEDLIST->buffer[LINKEDLISTITEM].theitem; \
  }
#endif

#if CHECK_CUSTOMBOUNDS
#define SCORECUSTOMLINKEDLIST_FRONT(LINKEDLIST, ELEM) \
  { \
    assert(LINKEDLIST->count > 0); \
    ELEM = LINKEDLIST->buffer[LINKEDLIST->head].theitem; \
  }
#else
#define SCORECUSTOMLINKEDLIST_FRONT(LINKEDLIST, ELEM) \
  { \
    ELEM = LINKEDLIST->buffer[LINKEDLIST->head].theitem; \
  }
#endif

#endif
