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
#ifndef _ScoreCInterface_H

#define _ScoreCInterface_H

#include "libperfctr.h"

#ifndef __cplusplus

// This is for backward-compatibility! No warranty it will work with new
// or any code!
// New code should use READ/WRITE version of these macros for time accounting.
/* ****no depth_hint***************/
#define NEW_SCORE_STREAM() \
   new_score_stream()
#define NEW_SIGNED_SCORE_STREAM(w) \
   new_signed_score_stream(w)
#define NEW_UNSIGNED_SCORE_STREAM(w) \
   new_unsigned_score_stream(w)
#define NEW_BOOLEAN_SCORE_STREAM() \
   new_boolean_score_stream()
#define NEW_SIGNED_FIXED_STREAM(i,f) \
   new_signed_fixed_stream(i,f)
#define NEW_UNSIGNED_FIXED_STREAM(i,f) \
   new_unsigned_fixed_stream(i,f)

#define NEW_READ_SCORE_STREAM() \
   new_read_score_stream()
#define NEW_READ_SIGNED_SCORE_STREAM(w) \
   new_read_signed_score_stream(w)
#define NEW_READ_UNSIGNED_SCORE_STREAM(w) \
   new_read_unsigned_score_stream(w)
#define NEW_READ_BOOLEAN_SCORE_STREAM() \
   new_read_boolean_score_stream()
#define NEW_READ_SIGNED_FIXED_STREAM(i,f) \
   new_read_signed_fixed_stream(i,f)
#define NEW_READ_UNSIGNED_FIXED_STREAM(i,f) \
   new_read_unsigned_fixed_stream(i,f)

#define NEW_WRITE_SCORE_STREAM() \
   new_write_score_stream()
#define NEW_WRITE_SIGNED_SCORE_STREAM(w) \
   new_write_signed_score_stream(w)
#define NEW_WRITE_UNSIGNED_SCORE_STREAM(w) \
   new_write_unsigned_score_stream(w)
#define NEW_WRITE_BOOLEAN_SCORE_STREAM() \
   new_write_boolean_score_stream()
#define NEW_WRITE_SIGNED_FIXED_STREAM(i,f) \
   new_write_signed_fixed_stream(i,f)
#define NEW_WRITE_UNSIGNED_FIXED_STREAM(i,f) \
   new_write_unsigned_fixed_stream(i,f)


/* ******depth_hint************************/
#define NEW_SCORE_STREAM_DEPTH_HINT(dh) \
   new_score_stream_depth_hint(dh)
#define NEW_SIGNED_SCORE_STREAM_DEPTH_HINT(w,dh) \
   new_signed_score_stream_depth_hint(w,dh)
#define NEW_UNSIGNED_SCORE_STREAM_DEPTH_HINT(w,dh) \
   new_unsigned_score_stream_depth_hint(w,dh)
#define NEW_BOOLEAN_SCORE_STREAM_DEPTH_HINT(dh) \
   new_boolean_score_stream_depth_hint(dh)
#define NEW_SIGNED_FIXED_STREAM_DEPTH_HINT(i,f,dh) \
   new_signed_fixed_stream_depth_hint(i,f,dh)
#define NEW_UNSIGNED_FIXED_STREAM_DEPTH_HINT(i,f,dh) \
   new_unsigned_fixed_stream_depth_hint(i,f,dh)

#define NEW_READ_SCORE_STREAM_DEPTH_HINT(dh) \
   new_read_score_stream_depth_hint(dh)
#define NEW_READ_SIGNED_SCORE_STREAM_DEPTH_HINT(w,dh) \
   new_read_signed_score_stream_depth_hint(w,dh)
#define NEW_READ_UNSIGNED_SCORE_STREAM_DEPTH_HINT(w,dh) \
   new_read_unsigned_score_stream_depth_hint(w,dh)
#define NEW_READ_BOOLEAN_SCORE_STREAM_DEPTH_HINT(dh) \
   new_read_boolean_score_stream_depth_hint(dh)
#define NEW_READ_SIGNED_FIXED_STREAM_DEPTH_HINT(i,f,dh) \
   new_read_signed_fixed_stream_depth_hint(i,f,dh)
#define NEW_READ_UNSIGNED_FIXED_STREAM_DEPTH_HINT(i,f,dh) \
   new_read_unsigned_fixed_stream_depth_hint(i,f,dh)

#define NEW_WRITE_SCORE_STREAM_DEPTH_HINT(dh) \
   new_write_score_stream_depth_hint(dh)
#define NEW_WRITE_SIGNED_SCORE_STREAM_DEPTH_HINT(w,dh) \
   new_write_signed_score_stream_depth_hint(w,dh)
#define NEW_WRITE_UNSIGNED_SCORE_STREAM_DEPTH_HINT(w,dh) \
   new_write_unsigned_score_stream_depth_hint(w,dh)
#define NEW_WRITE_BOOLEAN_SCORE_STREAM_DEPTH_HINT(dh) \
   new_write_boolean_score_stream_depth_hint(dh)
#define NEW_WRITE_SIGNED_FIXED_STREAM_DEPTH_HINT(i,f,dh) \
   new_write_signed_fixed_stream_depth_hint(i,f,dh)
#define NEW_WRITE_UNSIGNED_FIXED_STREAM_DEPTH_HINT(i,f,dh) \
   new_write_unsigned_fixed_stream_depth_hint(i,f,dh)

/* *****end of depth_hint***********************/

#define STREAM_WRITE(x,y) \
  { \
    unsigned long long ___t; \
    unsigned long long ___u,___v; \
    unsigned long long ___b, ___e; \
    rdtscll(___t); \
    ___b = usr_cpu_state->sum.tsc + (___t - usr_cpu_state->start.tsc); \
    rdtscll(___u); \
    ___v = ___u - ___t; \
    stream_write(x,y,___b); \
    rdtscll(___t); \
    ___e = usr_cpu_state->sum.tsc + (___t - usr_cpu_state->start.tsc); \
    *totalStreamOverheadPtr  += ((___e - ___b) + ___v + STREAM_OVERHEAD+66); \
    rdtscll(___u); \
    *totalStreamOverheadPtr  += (___u - ___t); \
  }

#define STREAM_READ(x,y) \
  { \
    unsigned long long ___t; \
    unsigned long long ___u,___v; \
    unsigned long long ___b, ___e; \
    rdtscll(___t); \
    ___b = usr_cpu_state->sum.tsc + (___t - usr_cpu_state->start.tsc); \
    rdtscll(___u); \
    ___v =___u - ___t; \
    y = stream_read(x,___b); \
    rdtscll(___t); \
    ___e = usr_cpu_state->sum.tsc + (___t - usr_cpu_state->start.tsc); \
    *totalStreamOverheadPtr  += ((___e - ___b) + ___v + STREAM_OVERHEAD+66); \
    rdtscll(___u); \
    *totalStreamOverheadPtr  += (___u - ___t); \
  }

#define STREAM_WRITE_NOACC(x,y) \
  stream_write(x,y,0)

#define STREAM_READ_NOACC(x) \
  stream_read(x,0)

#define STREAM_FREE(x) \
   stream_free(x)
#define STREAM_CLOSE(x) \
   stream_close(x)
#define STREAM_EOS(x) \
   stream_eos(x)
#define STREAM_DATA(x) \
   stream_data(x)
#define STREAM_FULL(x) \
   stream_full(x)
#define STREAM_EMPTY(x) \
   stream_empty(x)

typedef void* SCORE_STREAM;
typedef void* BOOLEAN_SCORE_STREAM;
typedef void* SIGNED_SCORE_STREAM;
typedef void* UNSIGNED_SCORE_STREAM;
typedef void* SIGNED_FIXED_STREAM;
typedef void* UNSIGNED_FIXED_STREAM;

#define NEW_SCORE_SEGMENT(n,w) \
   new_score_segment(n,w)
#define NEW_SIGNED_SCORE_SEGMENT(n,w) \
   new_signed_score_segment(n,w)
#define NEW_UNSIGNED_SCORE_SEGMENT(n,w) \
   new_unsigned_score_segment(n,w)
#define NEW_BOOLEAN_SCORE_SEGMENT(n,w) \
   new_boolean_score_segment(n,w)
#define NEW_SIGNED_FIXED_SCORE_SEGMENT(n,w) \
   new_signed_fixed_score_segment(n,w)
#define NEW_UNSIGNED_FIXED_SCORE_SEGMENT(n,w) \
   new_unsigned_fixed_score_segment(n,w)

#define GET_SEGMENT_DATA(x) \
   get_segment_data(x)

typedef void* SCORE_SEGMENT;
typedef void* BOOLEAN_SCORE_SEGMENT;
typedef void* SIGNED_SCORE_SEGMENT;
typedef void* UNSIGNED_SCORE_SEGMENT;
typedef void* SIGNED_FIXED_SCORE_SEGMENT;
typedef void* UNSIGNED_FIXED_SCORE_SEGMENT;


void *new_score_stream();
void *new_signed_score_stream(int w);
void *new_unsigned_score_stream(int w);
void *new_boolean_score_stream(int w);
void *new_signed_fixed_stream(int i, int f);
void *new_unsigned_fixed_stream(int i, int f);

void *new_read_score_stream();
void *new_read_signed_score_stream(int w);
void *new_read_unsigned_score_stream(int w);
void *new_read_boolean_score_stream(int w);
void *new_read_signed_fixed_stream(int i, int f);
void *new_read_unsigned_fixed_stream(int i, int f);

void *new_write_score_stream();
void *new_write_signed_score_stream(int w);
void *new_write_unsigned_score_stream(int w);
void *new_write_boolean_score_stream(int w);
void *new_write_signed_fixed_stream(int i, int f);
void *new_write_unsigned_fixed_stream(int i, int f);

void stream_write(void *x, long long int y, unsigned long long b);
long long int stream_read(void *x, unsigned long long b);

void stream_free(void *x);
void stream_close(void *x);
int stream_eos(void *x);
int stream_data(void *x);
int stream_full(void *x);
int stream_empty(void *x);

void *new_score_segment(int n, int w);
void *new_signed_score_segment(int n, int w);
void *new_unsigned_score_segment(int n, int w);
void *new_boolean_score_segment(int n, int w);
void *new_signed_fixed_score_segment(int n, int w);
void *new_unsigned_fixed_score_segment(int n, int w);

void *get_segment_data(void *x);

// HACK! FOR NOW!
#define SCORE_MARKREADSTREAM(x) \
   score_markreadstream(x)
#define SCORE_MARKWRITESTREAM(x) \
   score_markwritestream(x)

void score_markreadstream(void *x);
void score_markwritestream(void *x);

extern ScoreGlobalCounter *globalCounter;
extern unsigned long long *totalStreamOverheadPtr;
extern volatile const struct vperfctr_state *usr_kstate;

#endif

#endif
