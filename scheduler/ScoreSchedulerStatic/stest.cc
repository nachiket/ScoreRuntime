// Elaine Cheong
// 28 July 2000
// stest.cc

// test program for ScoreSSched

#include <iostream.h>
#include "ScoreSSched.h"

int main() {
  // number of pages and segments available in system
  const unsigned int numPhysicalCP = 16;
  const unsigned int numPhysicalCMB = 16;

  // make a new empty schedule
  ScoreSSched ssched(numPhysicalCP, numPhysicalCMB);

  // see what is in the schedule
  ssched.print();

  // add 3 slices and set number of cycles to run each slice0
  for (int i = 0; i < 3; i++) {
    sschedSlice_t *tempslice = NULL;
    if ((tempslice = ssched.addSlice()) == NULL) {
      cout << "failed" << endl;
      exit(0);
    }
    tempslice->cyclesToRun = (i + 1) * 1000;
  }

  ssched.print();

  for (int i = 0; i < 5; i++) {
    sschedSlice_t *tempslice;
    tempslice = ssched.getNextSlice();
    cout << endl << "tempslice.cyclesToRun " << tempslice->cyclesToRun  << endl;
  }

  ssched.addSlice();

  for (int i = 0; i < 5; i++) {
    sschedSlice_t *tempslice;
    tempslice = ssched.getNextSlice();
    cout << endl << "tempslice.cyclesToRun " << tempslice->cyclesToRun  << endl;
  }

  // clear schedule
  ssched.deleteAllSlices();

  ssched.print();

  for (int i = 0; i < 3; i++) {
    if (ssched.addSlice() == NULL) {
      cout << "failed" << endl;
      exit(0);
    }
  }

  ssched.print();

  return(0);
}
