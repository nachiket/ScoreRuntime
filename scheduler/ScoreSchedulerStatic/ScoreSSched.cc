// Elaine Cheong
// 25 Jul 2000
// ScoreSSched.cc

#include "ScoreSSched.h"
#include "ScorePage.h"
#include "ScoreSegment.h"

// Constructor
ScoreSSched::ScoreSSched(unsigned int numPagesAvailable, 
                         unsigned int numSegmentsAvailable) {
  cerr << "calling ScoreSSched constructor\n";
  numPages = numPagesAvailable;
  numSegments = numSegmentsAvailable;
  currentSlice = NULL;
} 

ScoreSSched::~ScoreSSched() {
  cerr << "calling ScoreSSched destructor\n";
  deleteAllSlices();
}

// Adds a new blank timeslice to the schedule
// Input: none
// Output: returns pointer to new timeslice
sschedSlice_t *ScoreSSched::addSlice() {
  sschedSlice_t *newSlice = new sschedSlice_t;

  // set up pages
  ScorePage **newPageList = new ScorePage* [numPages];
  for (unsigned int i = 0; i < numPages; i++) {
    newPageList[i] = NULL;
  }                                   
  newSlice->pageList = newPageList;


  // set up segments
  ScoreSegment **newSegmentList = new ScoreSegment* [numSegments];
  for (unsigned int i = 0; i < numSegments; i++) {
    newSegmentList[i] = NULL;
  }                                   
  newSlice->segmentList = newSegmentList;


  // initialize number of cycles to run
  newSlice->cyclesToRun = 0;

  // add new slice to list
  sschedList.push(newSlice);

  return newSlice;
}

// Delete all timeslices in static schedule
// Input: none
// Output: none
void ScoreSSched::deleteAllSlices() {
  sschedSlice_t *tempSlice;

  while (sschedList.length() > 0) {
    cerr << "delete: " << sschedList.length() << endl;
    tempSlice = sschedList.pop();
    delete [] tempSlice->pageList;
    delete [] tempSlice->segmentList;
    delete tempSlice;
  }

  currentSlice = NULL;

}


// Return next timeslice to run
// Input: none
// Output: returns pointer to next timeslice for scheduling
sschedSlice_t *ScoreSSched::getNextSlice() {
  // if not currently pointing at a slice, get first slice
  // else advance to next slice
  if (currentSlice == NULL) {
    currentSlice = sschedList.first();
  } else {
    currentSlice = sschedList.cyclic_succ(currentSlice);
  }

  sschedSlice_t *sliceToSchedule = sschedList.contents(currentSlice);

  return sliceToSchedule;
}

// Print all slices for debugging purposes
// Input: none
// Output: all timeslice info in static schedule
void ScoreSSched::print() {
  cerr << "print:\n";
  if (sschedList.length() > 0) {
    slist_item tempitem = sschedList.first();

    for (int i = 0; i < sschedList.length(); i++) {
      sschedSlice_t *tempSlice = sschedList.contents(tempitem);
      
      cerr << "cyclesToRun = " << tempSlice->cyclesToRun << endl;

      cerr << "pageList = ";
      for (unsigned int j = 0; j < numPages; j++) {
        cerr << tempSlice->pageList[j] << " ";
      }
      cerr << endl;


      cerr << "segmentList = ";
      for (unsigned int j = 0; j < numSegments; j++) {
        cerr << tempSlice->segmentList[j] << " ";
      }
      cerr << endl;

      tempitem = sschedList.succ(tempitem);
    }

  }
  cerr << "end print\n\n";
}
