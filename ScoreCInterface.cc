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

#include "ScoreStream.h"
#include "ScoreSegment.h"
#include "ScoreGlobalCounter.h"
#include "ScoreConfig.h"

/******no_depth_hint************/
extern "C" void *new_score_stream() {
  return((void *) NEW_SCORE_STREAM());
}

extern "C" void *new_signed_score_stream(int w) {
  return((void *) NEW_SIGNED_SCORE_STREAM(w));
}

extern "C" void *new_unsigned_score_stream(int w) {
  return((void *) NEW_UNSIGNED_SCORE_STREAM(w));
}

extern "C" void *new_boolean_score_stream() {
  return((void *) NEW_BOOLEAN_SCORE_STREAM());
}

extern "C" void *new_signed_fixed_stream(int i, int f) {
  return((void *) NEW_SIGNED_FIXED_STREAM(i, f));
}

extern "C" void *new_unsigned_fixed_stream(int i, int f) {
  return((void *) NEW_UNSIGNED_FIXED_STREAM(i, f));
}

extern "C" void *new_read_score_stream() {
  return((void *) NEW_READ_SCORE_STREAM());
}

extern "C" void *new_read_signed_score_stream(int w) {
  return((void *) NEW_READ_SIGNED_SCORE_STREAM(w));
}

extern "C" void *new_read_unsigned_score_stream(int w) {
  return((void *) NEW_READ_UNSIGNED_SCORE_STREAM(w));
}

extern "C" void *new_read_boolean_score_stream() {
  return((void *) NEW_READ_BOOLEAN_SCORE_STREAM());
}

extern "C" void *new_read_signed_fixed_stream(int i, int f) {
  return((void *) NEW_READ_SIGNED_FIXED_STREAM(i, f));
}

extern "C" void *new_read_unsigned_fixed_stream(int i, int f) {
  return((void *) NEW_READ_UNSIGNED_FIXED_STREAM(i, f));
}

extern "C" void *new_write_score_stream() {
  return((void *) NEW_WRITE_SCORE_STREAM());
}

extern "C" void *new_write_signed_score_stream(int w) {
  return((void *) NEW_WRITE_SIGNED_SCORE_STREAM(w));
}

extern "C" void *new_write_unsigned_score_stream(int w) {
  return((void *) NEW_WRITE_UNSIGNED_SCORE_STREAM(w));
}

extern "C" void *new_write_boolean_score_stream() {
  return((void *) NEW_WRITE_BOOLEAN_SCORE_STREAM());
}

extern "C" void *new_write_signed_fixed_stream(int i, int f) {
  return((void *) NEW_WRITE_SIGNED_FIXED_STREAM(i, f));
}

extern "C" void *new_write_unsigned_fixed_stream(int i, int f) {
  return((void *) NEW_WRITE_UNSIGNED_FIXED_STREAM(i, f));
}

/******end of no_depth_hint*************/

/******depth_hint************/
extern "C" void *new_score_stream_depth_hint(int dh) {
  return((void *) NEW_SCORE_STREAM_DEPTH_HINT(dh));
}

extern "C" void *new_signed_score_stream_depth_hint(int w, int dh) {
  return((void *) NEW_SIGNED_SCORE_STREAM_DEPTH_HINT(w, dh));
}

extern "C" void *new_unsigned_score_stream_depth_hint(int w, int dh) {
  return((void *) NEW_UNSIGNED_SCORE_STREAM_DEPTH_HINT(w, dh));
}

extern "C" void *new_boolean_score_stream_depth_hint(int dh) {
  return((void *) NEW_BOOLEAN_SCORE_STREAM_DEPTH_HINT(dh));
}

extern "C" void *new_signed_fixed_stream_depth_hint(int i, int f, int dh) {
  return((void *) NEW_SIGNED_FIXED_STREAM_DEPTH_HINT(i, f, dh));
}

extern "C" void *new_unsigned_fixed_stream_depth_hint(int i, int f, int dh) {
  return((void *) NEW_UNSIGNED_FIXED_STREAM_DEPTH_HINT(i, f, dh));
}

extern "C" void *new_read_score_stream_depth_hint(int dh) {
  return((void *) NEW_READ_SCORE_STREAM_DEPTH_HINT(dh));
}

extern "C" void *new_read_signed_score_stream_depth_hint(int w, int dh) {
  return((void *) NEW_READ_SIGNED_SCORE_STREAM_DEPTH_HINT(w, dh));
}

extern "C" void *new_read_unsigned_score_stream_depth_hint(int w, int dh) {
  return((void *) NEW_READ_UNSIGNED_SCORE_STREAM_DEPTH_HINT(w, dh));
}

extern "C" void *new_read_boolean_score_stream_depth_hint(int dh) {
  return((void *) NEW_READ_BOOLEAN_SCORE_STREAM_DEPTH_HINT(dh));
}

extern "C" void *new_read_signed_fixed_stream_depth_hint(int i, int f, int dh) {
  return((void *) NEW_READ_SIGNED_FIXED_STREAM_DEPTH_HINT(i, f, dh));
}

extern "C" void *new_read_unsigned_fixed_stream_depth_hint(int i, int f, int dh) {
  return((void *) NEW_READ_UNSIGNED_FIXED_STREAM_DEPTH_HINT(i, f, dh));
}

extern "C" void *new_write_score_stream_depth_hint(int dh) {
  return((void *) NEW_WRITE_SCORE_STREAM_DEPTH_HINT(dh));
}

extern "C" void *new_write_signed_score_stream_depth_hint(int w, int dh) {
  return((void *) NEW_WRITE_SIGNED_SCORE_STREAM_DEPTH_HINT(w, dh));
}

extern "C" void *new_write_unsigned_score_stream_depth_hint(int w, int dh) {
  return((void *) NEW_WRITE_UNSIGNED_SCORE_STREAM_DEPTH_HINT(w, dh));
}

extern "C" void *new_write_boolean_score_stream_depth_hint(int dh) {
  return((void *) NEW_WRITE_BOOLEAN_SCORE_STREAM_DEPTH_HINT(dh));
}

extern "C" void *new_write_signed_fixed_stream_depth_hint(int i, int f, int dh) {
  return((void *) NEW_WRITE_SIGNED_FIXED_STREAM_DEPTH_HINT(i, f, dh));
}

extern "C" void *new_write_unsigned_fixed_stream_depth_hint(int i, int f, int dh) {
  return((void *) NEW_WRITE_UNSIGNED_FIXED_STREAM_DEPTH_HINT(i, f, dh));
}

/******depth_hint*************/


extern "C" void stream_write(void *x, long long int y, unsigned long long b) {
  ((SCORE_STREAM) x)->stream_write(y, 0, b);
}

extern "C" long long int stream_read(void *x, unsigned long long b) {
  return(((SCORE_STREAM) x)->stream_read(b));
}

extern "C" void stream_free(void *x) {
  return(STREAM_FREE(((SCORE_STREAM) x)));
}

extern "C" void stream_close(void *x) {
  return(STREAM_CLOSE(((SCORE_STREAM) x)));
}

extern "C" int stream_eos(void *x) {
  return(STREAM_EOS(((SCORE_STREAM) x)));
}

extern "C" int stream_data(void *x) {
  return(STREAM_DATA(((SCORE_STREAM) x)));
}

extern "C" int stream_full(void *x) {
  return(STREAM_FULL(((SCORE_STREAM) x)));
}

extern "C" int stream_empty(void *x) {
  return(STREAM_EMPTY(((SCORE_STREAM) x)));
}

extern "C" void *new_score_segment(int n, int w) {
  return((void *) NEW_SCORE_SEGMENT(n, w));
}

extern "C" void *new_signed_score_segment(int n, int w) {
  return((void *) NEW_SIGNED_SCORE_SEGMENT(n, w));
}

extern "C" void *new_unsigned_score_segment(int n, int w) {
  return((void *) NEW_UNSIGNED_SCORE_SEGMENT(n, w));
}

extern "C" void *new_boolean_score_segment(int n, int w) {
  return((void *) NEW_BOOLEAN_SCORE_SEGMENT(n, w));
}

extern "C" void *new_signed_fixed_score_segment(int n, int w) {
  return((void *) NEW_SIGNED_FIXED_SCORE_SEGMENT(n, w));
}

extern "C" void *new_unsigned_fixed_score_segment(int n, int w) {
  return((void *) NEW_UNSIGNED_FIXED_SCORE_SEGMENT(n, w));
}

extern "C" void *get_segment_data(void *x) {
  return(((SCORE_SEGMENT) x)->data());
}

extern "C" void score_markreadstream(void *x) {
  ((ScoreStream *) x)->setReadThreadCounter(globalCounter->threadCounter);
}

extern "C" void score_markwritestream(void *x) {
  ((ScoreStream *) x)->setWriteThreadCounter(globalCounter->threadCounter);
}

