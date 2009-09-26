// Elaine Cheong
// 25 Jul 2000
// ScoreSSched.h

// Data structure to hold static schedule

#ifndef _ScoreSSched_H
#define _ScoreSSched_H

#include "LEDA/slist.h"
#include "ScorePage.h"
#include "ScoreSegment.h"

// Data structure to hold one timeslice in static schedule
typedef struct _sschedSlice_t {
  ScorePage **pageList;            // list of pages to run in this timeslice
  ScoreSegment **segmentList;      // list of segments to run in this timeslice
  unsigned int cyclesToRun;        // number of cycles to run this timeslice
} sschedSlice_t;


class ScoreSSched {
 public: 
  ScoreSSched(unsigned int numPagesAvailable, unsigned int numSegmentsAvailable);
  ~ScoreSSched();

  sschedSlice_t *addSlice();       // add a new blank timeslice to the schedule
  void deleteAllSlices();          // delete all timeslices in static schedule
  sschedSlice_t *getNextSlice();   // return next timeslice to run

  void print();                    // print all slices

 private:
  unsigned int numPages;           // num of pages in system (= numPhysicalCP)
  unsigned int numSegments;        // num of segments in system (= numPhysicalCMB)
  slist<sschedSlice_t *> sschedList; // list of timeslices for static schedule
  slist_item currentSlice;         // container for timeslice to be scheduled

 protected:

};

#endif

