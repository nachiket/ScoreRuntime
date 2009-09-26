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
// SCORE Simulator
// $Revision: 2.2 $
//
//////////////////////////////////////////////////////////////////////////////

#include <string.h>

#include "ScoreAccountingReconfigCmds.h"

extern char *eventNameMap[];


ScoreAccountingReconfigCmds::ScoreAccountingReconfigCmds()
{
  _historyList = new ScoreCustomList<int*>(MAX_TS);

  memset (_prevStats, 0, sizeof (_prevStats));
}


ScoreAccountingReconfigCmds::~ScoreAccountingReconfigCmds()
{
  for (unsigned int i = 0; i < SCORECUSTOMLIST_LENGTH(_historyList); i ++) {
    int *tmp;
    SCORECUSTOMLIST_ITEMAT(_historyList, i, tmp);
    delete tmp;
  }
}

void ScoreAccountingReconfigCmds::recordNextTS(int *stats)
{
  int *newRecord = new int[NumberOf_SCORE_EVENT];
  for (int i = 0; i < NumberOf_SCORE_EVENT; i ++) {
    newRecord[i] = stats[i] - _prevStats[i];
  }

  memcpy(_prevStats, stats, sizeof(_prevStats));
  SCORECUSTOMLIST_APPEND(_historyList, newRecord);
}

void ScoreAccountingReconfigCmds::dumpToFile(FILE *f) 
{
  if (!f)
    return;

  fprintf(f, "Event Name");
  for (unsigned int i = 1; i < SCORECUSTOMLIST_LENGTH(_historyList); i ++) {
    fprintf(f, "\t%d", i);
  }
  fprintf(f, "\tAve\tStdDev\n");

  for (int j = 0; j < NumberOf_SCORE_EVENT; j ++) {
    fprintf(f, "%s", eventNameMap[j]);
    float total = 0;
    for (unsigned int i = 1; i < SCORECUSTOMLIST_LENGTH(_historyList); i ++) {
      int *currTS;
      SCORECUSTOMLIST_ITEMAT(_historyList, i, currTS);
      fprintf(f, "\t%d", currTS[j]);
      total += currTS[j];
    }
    float ave = total / (SCORECUSTOMLIST_LENGTH(_historyList) - 1);

    fprintf(f, "\t%.1f", ave);

    float sumOfSqDiffs = 0;
     
    for (unsigned int i = 1; i < SCORECUSTOMLIST_LENGTH(_historyList); i ++) {
      int *currTS;
      SCORECUSTOMLIST_ITEMAT(_historyList, i, currTS);
      sumOfSqDiffs += (currTS[j] - ave) * (currTS[j] - ave);
    }

    float stddev = sqrt(sumOfSqDiffs / 
			(SCORECUSTOMLIST_LENGTH(_historyList) - 2));

    fprintf(f, "\t%.3f", stddev);

    fprintf(f, "\n");
  }
  
  fclose(f);
}
