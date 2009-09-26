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
// SCORE Visualization Interface
// $Revision: 1.6 $
//
//////////////////////////////////////////////////////////////////////////////

#ifndef _ScoreVisualization_H
#define _ScoreVisualization_H

#include <iostream>
#include <stdio.h>


// defines various parameters.
#define VISUALIZATION_MAJOR_VERSION          1
#define VISUALIZATION_MINOR_VERSION          1
#define VISUALIZATION_MAX_CPCMBNAME_LENGTH   20
#define VISUALIZATION_MAX_EVENTNAME_LENGTH   20
#define VISUALIZATION_EVENTBUFFER_LENGTH     1000
#define VISUALIZATION_INT_SIZE               4
#define VISUALIZATION_LONG_SIZE              8


// defines the types of visualization events we want to log.
#define VISUALIZATION_EVENT_IDLE                         0
#define VISUALIZATION_EVENT_FIRING                       1
#define VISUALIZATION_EVENT_DUMPPAGESTATE                2
#define VISUALIZATION_EVENT_DUMPPAGEFIFO                 3
#define VISUALIZATION_EVENT_LOADPAGECONFIG               4
#define VISUALIZATION_EVENT_LOADPAGESTATE                5
#define VISUALIZATION_EVENT_LOADPAGEFIFO                 6
#define VISUALIZATION_EVENT_DUMPSEGMENTFIFO              7
#define VISUALIZATION_EVENT_LOADSEGMENTFIFO              8
#define VISUALIZATION_EVENT_MEMXFERPRIMARYTOCMB          9
#define VISUALIZATION_EVENT_MEMXFERCMBTOPRIMARY          10
#define VISUALIZATION_EVENT_MEMXFERCMBTOCMB              11
#define VISUALIZATION_EVENT_GETSEGMENTPOINTERS           12
#define VISUALIZATION_EVENT_SETSEGMENTCONFIGPOINTERS     13
#define VISUALIZATION_EVENT_CHANGESEGMENTMODE            14
#define VISUALIZATION_EVENT_CHANGESEGMENTTRAANDPBOANDMAX 15
#define VISUALIZATION_EVENT_RESETSEGMENTDONEFLAG         16
#define VISUALIZATION_EVENT_CONNECTSTREAM                17


// visualization manager.
class ScoreVisualization {
 public:
  ScoreVisualization(int newNumCPs, int newNumCMBs);
  ~ScoreVisualization();
  void startVisualization(char *filename);
  void syncVisualizationToFile();
  void stopVisualization();
  void stopVisualization_noclose();
  void addEventCP(int id, int event, int time);
  void addEventCMB(int id, int event, int time);

 private:
  int numCPs, numCMBs;
  FILE *logfile;
  char *eventBuffer;
  int eventBufferIndex;

  void write(char *buffer, unsigned int length);
  void writeInt(int data);
  void writeIntToBuffer(int data);
  void writeLongToBuffer(long data);
};

#endif
