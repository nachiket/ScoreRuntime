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

#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <string.h>
#include <errno.h>
#include "ScoreVisualization.h"
#include "ScoreConfig.h"

using std::cout;
using std::endl;
using std::cerr;

// this is the array of visualization event names that should correspond to
// the indices defined in ScoreVisualization.h!
static char *visualizationEventNameTable[] =
{
  "IDLE",
  "FIRING",
  "DUMPPAGESTATE", 
  "DUMPPAGEFIFO",
  "LOADPAGECONFIG",
  "LOADPAGESTATE",
  "LOADPAGEFIFO",
  "DUMPSEGMENTFIFO",
  "LOADSEGMENTFIFO",
  "MEMXFERPRIMARYTOCMB",
  "MEMXFERCMBTOPRIMARY",
  "MEMXFERCMBTOCMB",
  "GETSEGMENTPOINTERS",
  "SETSEGMENTCONFIGPOINTERS",
  "CHANGESEGMENTMODE",
  "CHANGESEGMENTTRAANDPBOANDMAX",
  "RESETSEGMENTDONEFLAG",
  "CONNECTSTREAM"
};

// defines the number of visualization events.
#define NUMBER_OF_VISUALIZATION_EVENTS       18


// start a visualization manager.
ScoreVisualization::ScoreVisualization(int newNumCPs, int newNumCMBs) {
  // create the event buffer.
  eventBuffer = (char *) malloc(VISUALIZATION_EVENTBUFFER_LENGTH*
				(VISUALIZATION_INT_SIZE+
				 VISUALIZATION_LONG_SIZE+
				 VISUALIZATION_INT_SIZE));
  eventBufferIndex = 0;

  logfile = NULL;

  numCPs = newNumCPs;
  numCMBs = newNumCMBs;
}


// clean up visualization manager.
ScoreVisualization::~ScoreVisualization() {
  // stop any visualization still going on.
  if (logfile != NULL) {
    stopVisualization();
  }

  // free the event buffer.
  free(eventBuffer);
  eventBuffer = NULL;
  eventBufferIndex = -1;
}


// start the logging of a new visualization to the specified file.
void ScoreVisualization::startVisualization(char *filename) {
  int i;


  // make sure a log file isn't already open!
  if (logfile != NULL) {
    cerr << "SCORE_VISUALIZATION: Attempting to open a new log file before " <<
      "closing existing one!" << endl;
    exit(1);
  }

  // attempt to open a log file for writing.
  if (filename == NULL) {
    cerr << "SCORE_VISUALIZATION: Filename for log file was NULL!" << endl;
    exit(1);
  }
  logfile = fopen(filename, "w");
  if (logfile == NULL) {
    cerr << "SCORE_VISUALIZATION: Could not open log file " << filename <<
      " for writing! (errno=" << errno << ")!" << endl;
    exit(1);
  }

  // write the version.
  writeInt(VISUALIZATION_MAJOR_VERSION);
  writeInt(VISUALIZATION_MINOR_VERSION);

  // write out number of CPs and CMBs.
  writeInt(numCPs+numCMBs);

  // write out number of events.
  writeInt(NUMBER_OF_VISUALIZATION_EVENTS);

  // write out the event names.
  for (i = 0; i < NUMBER_OF_VISUALIZATION_EVENTS; i++) {
    char *eventName = visualizationEventNameTable[i];
    int nameLength = strlen(eventName);

    if (nameLength > VISUALIZATION_MAX_EVENTNAME_LENGTH) {
      nameLength = VISUALIZATION_MAX_EVENTNAME_LENGTH;
    }

    writeInt(nameLength);
    write(eventName, nameLength);
  }

  // write out the names for the CPs and CMBs.
  for (i = 0; i < numCPs; i++) {
    char cpName[VISUALIZATION_MAX_CPCMBNAME_LENGTH];
    int nameLength;

    sprintf(cpName, "CP%d", i);
    nameLength = strlen(cpName);

    if (nameLength > VISUALIZATION_MAX_CPCMBNAME_LENGTH) {
      nameLength = VISUALIZATION_MAX_CPCMBNAME_LENGTH;
    }

    writeInt(nameLength);
    write(cpName, nameLength);
  }
  for (i = 0; i < numCMBs; i++) {
    char cmbName[VISUALIZATION_MAX_CPCMBNAME_LENGTH];
    int nameLength;

    sprintf(cmbName, "CMB%d", i);
    nameLength = strlen(cmbName);

    if (nameLength > VISUALIZATION_MAX_CPCMBNAME_LENGTH) {
      nameLength = VISUALIZATION_MAX_CPCMBNAME_LENGTH;
    }

    writeInt(nameLength);
    write(cmbName, nameLength);
  }
}


// write the outstanding events to the file (does not close the log).
// NOTE: We may want to optimize this by having addEvent transform all
//       events to the proper byte-level representation, and this would
//       then just be a big
void ScoreVisualization::syncVisualizationToFile() {
  // make sure there is a logfile!
  if (logfile == NULL) {
    cerr << "SCORE_VISUALIZATION: Trying to sync a non-existant log file!" <<
      endl;
    exit(1);
  }

  // write all of the outstanding events to the logfile.
  write(eventBuffer, eventBufferIndex);

  // reset the index.
  eventBufferIndex = 0;

  // flush the logfile.
  fflush(logfile);
}


// write the outstanding events to the file and also close the log.
void ScoreVisualization::stopVisualization() {
  // make sure there is a logfile!
  if (logfile == NULL) {
    cerr << "SCORE_VISUALIZATION: Trying to close a non-existant log file!" <<
      endl;
    exit(1);
  }

  // if there are outstanding events, the write them to the file.
  if (eventBufferIndex > 0) {
    syncVisualizationToFile();
  }

  // close the logfile.
  fflush(logfile);
  fclose(logfile);
  logfile = NULL;
}


// write the outstanding events to the file and but does not close log.
// NOTE: This is for SIGINT handler since it doesn't seem to like fclose().
void ScoreVisualization::stopVisualization_noclose() {
  // make sure there is a logfile!
  if (logfile == NULL) {
    cerr << "SCORE_VISUALIZATION: Trying to close a non-existant log file!" <<
      endl;
    exit(1);
  }

  // if there are outstanding events, the write them to the file.
  if (eventBufferIndex > 0) {
    syncVisualizationToFile();
  }

  fflush(logfile);
  logfile = NULL;
}


// add a CP event to the event buffer (if the event buffer becomes full, then
// write the buffered events to the logfile first).
void ScoreVisualization::addEventCP(int id, int event, int time) {
  // make sure there is a logfile!
  if (logfile == NULL) {
    cerr << "SCORE_VISUALIZATION: Trying to add events to a non-existant " <<
      "log file!" << endl;
    exit(1);
  }

  // if the event buffer is full, dump that to the logfile first.
  if (eventBufferIndex == (VISUALIZATION_EVENTBUFFER_LENGTH*
			   (VISUALIZATION_INT_SIZE+
			    VISUALIZATION_LONG_SIZE+
			    VISUALIZATION_INT_SIZE))) {
    syncVisualizationToFile();
  }

  // add the event to the event buffer.
  writeIntToBuffer(id);
  writeLongToBuffer(time);
  writeIntToBuffer(event);
}


// add a CMB event to the event buffer (if the event buffer becomes full, then
// write the buffered events to the logfile first).
void ScoreVisualization::addEventCMB(int id, int event, int time) {
  // make sure there is a logfile!
  if (logfile == NULL) {
    cerr << "SCORE_VISUALIZATION: Trying to add events to a non-existant " <<
      "log file!" << endl;
    exit(1);
  }

  // adjust the id.
  id = id + numCPs;

  // if the event buffer is full, dump that to the logfile first.
  if (eventBufferIndex == (VISUALIZATION_EVENTBUFFER_LENGTH*
			   (VISUALIZATION_INT_SIZE+
			    VISUALIZATION_LONG_SIZE+
			    VISUALIZATION_INT_SIZE))) {
    syncVisualizationToFile();
  }

  // add the event to the event buffer.
  writeIntToBuffer(id);
  writeLongToBuffer(time);
  writeIntToBuffer(event);
}


// writes out the current character buffer to the log.
// NOTE: It is assumed that there is a logfile!
// NOTE: It is assumed that the buffer is not NULL and length is non-negative!
void ScoreVisualization::write(char *buffer, unsigned int length) {
  // if there is nothing to write, return.
  if (length == 0) {
    return;
  }

  // try to write the buffer to the logfile.
  if (fwrite(buffer, 1, length, logfile) != length) {
    cerr << "SCORE_VISUALIZATION: Could not write all of buffer to logfile!" <<
      endl;
    exit(1);
  }
}


// writes out an integer to the log (highest byte first) (4 bytes total).
// NOTE: It is assumed that there is a logfile!
void ScoreVisualization::writeInt(int data) {
  char buffer[VISUALIZATION_INT_SIZE];
  int i;


  // write the integer to the buffer in highest byte first order.
  for (i = 0; i < VISUALIZATION_INT_SIZE; i++) {
    buffer[VISUALIZATION_INT_SIZE-i-1] = (char) (data & 0xFF);
    data = data >> 8;
  }

  // write the buffer to the logfile.
  write(buffer, VISUALIZATION_INT_SIZE);
}


// writes out an integer to the event buffer (highest byte first) 
// (4 bytes total).
// NOTE: It is assumed that there is enough space in the buffer!
void ScoreVisualization::writeIntToBuffer(int data) {
  int i;


  // advance the eventBufferIndex.
  eventBufferIndex = eventBufferIndex + VISUALIZATION_INT_SIZE;

  // write the integer to the buffer in highest byte first order.
  for (i = 0; i < VISUALIZATION_INT_SIZE; i++) {
    eventBuffer[eventBufferIndex-i-1] = (char) (data & 0xFF);
    data = data >> 8;
  }
}


// writes out a long to the event buffer (highest byte first) (8 bytes total).
// NOTE: It is assumed that there is enough space in the buffer!
void ScoreVisualization::writeLongToBuffer(long data) {
  int i;


  // advance the eventBufferIndex.
  eventBufferIndex = eventBufferIndex + VISUALIZATION_LONG_SIZE;

  // write the long to the buffer in highest byte first order.
  for (i = 0; i < VISUALIZATION_LONG_SIZE; i++) {
    eventBuffer[eventBufferIndex-i-1] = (char) (data & 0xFF);
    data = data >> 8;
  }
}
