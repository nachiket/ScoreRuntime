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
// SCORE Segment Operator (Read-only) Instance
// $Revision: 1.3 $
//
//////////////////////////////////////////////////////////////////////////////

#include <stdlib.h>
#include <string.h>
#include "ScorePage.h"
#include "ScoreSegment.h"
#include "ScoreStream.h"
#include "ScoreConfig.h"
#include "ScoreSegmentReadOnly.h"
#include "ScoreSegmentOperatorReadOnly.h"
#include "ScoreSegmentOperatorReadOnly_instance.h"

extern "C" { void *__dso_handle= NULL; }

ScoreSegmentOperatorReadOnly_instance::ScoreSegmentOperatorReadOnly_instance(
  ScoreSegment *segPtr,
  ScoreStream *addr, ScoreStream *data) {
  ScoreSegment *segment_r_inst = new ScoreSegmentReadOnly(segPtr, addr, data);

  pages = 0;
  segments = 1;
  page = new ScorePage *[0];
  segment = new ScoreSegment *[1];
  page_group = new int[0];
  segment_group = new int[1];
  segment[0] = segment_r_inst;
  segment_group[0] = NO_GROUP; // FIX ME! SHOULD WE EVER GIVE THIS A GROUP??
}


extern "C" ScoreOperatorInstance *construct(char *argbuf) {
  ScoreSegmentOperatorReadOnly_arg *arg_data;

  arg_data = (ScoreSegmentOperatorReadOnly_arg *)
    malloc(sizeof(ScoreSegmentOperatorReadOnly_arg));
  memcpy(arg_data, argbuf, sizeof(ScoreSegmentOperatorReadOnly_arg));
  return(new ScoreSegmentOperatorReadOnly_instance(
           SEGMENT_ID_TO_OBJ(arg_data->segPtrID),
	   STREAM_ID_TO_OBJ(arg_data->addrID),
	   STREAM_ID_TO_OBJ(arg_data->dataID)));
}
