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
// $Revision: 1.52 $
//
//////////////////////////////////////////////////////////////////////////////

#include <signal.h>
#include "ScoreSegment.h"
#include "ScoreConfig.h"

#include <iostream> // added by Nachiket for newer file IO constructs
#include <iomanip> // added by Nachiket for newer file IO constructs
#include <ios> // added by Nachiket for newer file IO constructs

using std::ios;
using std::fstream;
using std::setfill;
using std::setw;

int ScoreSegment::currentID = 0;
int ScoreSegment::tempID = 0;
ScoreSegment *ScoreSegment::shmptr=0;
void *ScoreSegment::dataPtrTable[NUMOFSHAREDSEG] = {0};
void *ScoreSegment::dataRangeTable[NUMOFSHAREDSEG] = {0};
ScoreSegment *ScoreSegment::segPtrTable[NUMOFSHAREDSEG] = {0};

// want to initiate signal catching code
int ScoreSegment::initSig=initSigCatch();


void *ScoreSegment::operator new(size_t size) {

  // create a new shared segment and malloc

/* COMMENT_REMOVE on April 15th 2010 (Anyone reading this, DO YOUR FREAKING TAXES!!)
  // need to get new segmentID
  // since we don't have one for Segment ID, we will re-use /proc/streamid
  int file = (int) open("/proc/streamid",O_RDONLY);

  if (file < 0) {
    perror("ERROR: Could not open /proc/streamid ");
    exit(errno);
  }

  if (read(file, &tempID, 4) != 4) {
    perror("ERROR: Did not get 4 bytes of data back ");
    exit(errno);
  }
  close(file);
*/  // COMMENT_REMOVE

  fstream infile("/tmp/streamid",ios::in);
  if (infile.fail() || infile.bad()) {
	  cout << "Initialize /tmp/streamid" << endl;
	  infile.close();
	  // initialize the file..
	  fstream outfile("/tmp/streamid",ios::out);
	  outfile << setfill('0');
	  outfile << setw(4);
	  outfile << 1;
	  tempID=0;
	  outfile.close();
  } else {
	  char buffer[4]; //=(char*)malloc(4);
	  infile.read(buffer,4);
	  tempID=(buffer[3]-48)+
		  10*(buffer[2]-48)+
		  100*(buffer[1]-48)+
		  1000*(buffer[0]-48);
//	          cout << "Found existing /tmp/streamid: " << tempID << endl;
	  infile.close();

	  // Nachiket added update routine...
	  // need to get new streamID
	  fstream outfile("/tmp/streamid",ios::out);
	  outfile << setfill('0');
	  outfile << setw(4);
	  outfile << tempID+1;
	  outfile.close();

  }

  // Nachiket added update routine...
  // need to get new streamID
//  fstream outfile("/tmp/streamid",ios::out);
//  outfile << setfill('0');
//  outfile << setw(4);
//  outfile << tempID+1;
//  outfile.close();

  if(errno) {
	  perror("Something went wrong..");
	  exit(errno);
  }


  // need to allocate new segment of share memory

  if ((currentID=shmget((int)tempID, size, IPC_EXCL | IPC_CREAT | 0666)) != -1) {
    // need to attach the segment on the page boundary
    // this is done automatically already 
    if ((shmptr=(ScoreSegment *)shmat(currentID, 0, 0))==(ScoreSegment *) -1) {
      perror("shmptr -- segment new operator -- attach ");
      exit(errno);
    }
  }
  else {
    perror("currentID -- segment new operator -- creation ");
    exit(errno);
  }

  if (VERBOSEDEBUG || DEBUG) {
    cout << "   SegmentID is: " << currentID << "\n";
  }
//    cout << "   SegmentID is: " << currentID << " shmptr=" << shmptr << "\n";

  return shmptr;
}


void ScoreSegment::operator delete(void *p, size_t size) {
  ScoreSegment *currentSegment = (ScoreSegment *) p;

  if (currentSegment->getSegmentType() == SCORE_SEGMENT_PLAIN) {
    // FIX ME! NEED TO PROPERLY DELETE SHARED SEGMENTS!
    cerr << "SCORESEGMENTERR: DELETE OF SCORESEGMENT IS NOT IMPLEMENTED!" << 
      endl;
  } else if (currentSegment->getSegmentType() == SCORE_SEGMENT_READONLY) {
    ::delete p;
  } else if (currentSegment->getSegmentType() == SCORE_SEGMENT_STITCH) {
    ::delete p;
  }
}


///////////////////////////////////////////////////////////////////////////////
// ScoreSegment::ScoreSegment:
//   Constructor for ScoreSegment.
//   Initializes all internal structures.
//
// Parameters: 
//
// Return value: None.
///////////////////////////////////////////////////////////////////////////////

ScoreSegment::ScoreSegment() {
  sched_isStitch = 0;
  mode = 0;
  maxAddr = 0;
  traAddr = 0;
  pboAddr = 0;
  readAddr = 0;
  writeAddr = 0;
  sched_mode = 0;
  sched_maxAddr = 0;
  sched_traAddr = 0;
  sched_pboAddr = 0;
  sched_readAddr = 0;
  sched_writeAddr = 0;
  sched_readCount = 0;
  sched_writeCount = 0;
  sched_cachedSegmentBlock = NULL;
  sched_dumpSegmentBlock = NULL;
  sched_fifoBuffer = NULL;
  sched_isFIFOBufferValid = 0;
  sched_isFaulted = 0;
  sched_faultedAddr = 0;
  sched_residentStart = 0;
  sched_residentLength = 0;
  sim_isFaulted = 0;
  sim_faultedAddr = 0;
  sched_dumpOnDone = 0;
  readCount = 0;
  writeCount = 0;

  start_val[0] = 0;
  incrementuse.sem_num = SEG_USECOUNT;
  incrementuse.sem_op = 1;
  incrementuse.sem_flg = 0; 
  waitfornouse.sem_num = SEG_USECOUNT;
  waitfornouse.sem_op = 0;
  waitfornouse.sem_flg = 0; 
  decrementuse.sem_num = SEG_USECOUNT;
  decrementuse.sem_op = -1;
  decrementuse.sem_flg = 0; 

  sim_segmentInputMask = 0;

  this_segment_is_done = 0;
  sched_this_segment_is_done = 0;

  sched_old_mode = 0;
  dataPtr = (void*)11111;
  dataID = 2;

  shouldUseUSECOUNT = 0;

  _isPage = 0;
  _isOperator = 0;
  _isSegment = 1;
}

///////////////////////////////////////////////////////////////////////////////
// ScoreSegment::ScoreSegment:
//   Constructor for ScoreSegment.
//   Initializes all internal structures.
//
// Parameters: nlength, nwidth
//
// Return value: None.
///////////////////////////////////////////////////////////////////////////////
ScoreSegment::ScoreSegment(int nlength, int nwidth, ScoreType type_t) {

  segType = type_t;
  //segmentType = SCORE_SEGMENT_PLAIN;
  segmentType = 0;
  sched_isStitch = 0;
  mode = 0;
  maxAddr = 0;
  traAddr = 0;
  pboAddr = 0;
  readAddr = 0;
  writeAddr = 0;
  sched_mode = 0;
  sched_maxAddr = 0;
  sched_traAddr = 0;
  sched_pboAddr = 0;
  sched_readAddr = 0;
  sched_writeAddr = 0;
  sched_readCount = 0;
  sched_writeCount = 0;
  sched_cachedSegmentBlock = NULL;
  sched_dumpSegmentBlock = NULL;
  sched_fifoBuffer = NULL;
  sched_isFIFOBufferValid = 0;
  sched_isFaulted = 0;
  sched_faultedAddr = 0;
  sched_residentStart = 0;
  sched_residentLength = 0;
  sim_isFaulted = 0;
  sim_faultedAddr = 0;
  sched_dumpOnDone = 0;
  readCount = 0;
  writeCount = 0;
  
  recycleID0 = tempID;
  segmentID = currentID;
  segPtr = shmptr;

  segLength = nlength;
  segWidth = nwidth;
  segSize = (int)segLength * 8; // make everything long long integer sized.

  // need to make sure segPtr is page align
  // the "data" section will be another shared memory segment
  // it will be page aligned when attached to a process

/*
  // open /proc/streamid to get a unique number
  int file = (int) open("/proc/streamid",O_RDONLY);

  if (file < 0) {
    perror("ERROR: Could not open /proc/streamid ");
    exit(errno);
  }

  // the unique number will be stored in recycleID1
  // so we can recycle this number when we are done 
  if (read(file, &recycleID1, 4) != 4) {
    perror("ERROR: Did not get 4 bytes of data back ");
    exit(errno);
  }
  close(file);
*/

  int newID=0;

  fstream infile("/tmp/streamid",ios::in);
  if (infile.fail() || infile.bad()) {
	  cout << "Initialize /tmp/streamid" << endl;
	  infile.close();
	  // initialize the file..
	  fstream outfile("/tmp/streamid",ios::out);
	  outfile << setfill('0');
	  outfile << setw(4);
	  outfile << 1;
	  newID=0;
	  outfile.close();
  } else {
	  char buffer1[4]; //=(char*)malloc(4);
	  infile.read(buffer1,4);
	  newID=(buffer1[3]-48)+
		  10*(buffer1[2]-48)+
		  100*(buffer1[1]-48)+
		  1000*(buffer1[0]-48);
//	          cout << "Found existing /tmp/streamid: " << recycleID1 << endl;
	  infile.close();

	  // Nachiket added update routine...
	  // need to get new streamID
	  fstream outfile("/tmp/streamid",ios::out);
	  outfile << setfill('0');
	  outfile << setw(4);
	  outfile << newID+1;
	  outfile.close();

  }
  
  recycleID1=0;
  recycleID1=newID;

  // Nachiket added update routine...
  // need to get new streamID
//  fstream outfile("/tmp/streamid",ios::out);
//  outfile << setfill('0');
//  outfile << setw(4);
//  outfile << recycleID1+1;
//  outfile.close();

  if(errno) {
	  perror("Something went wrong..");
	  exit(errno);
  }


  // create a share memory segment and stored the segment ID in dataID
  if ((dataID=shmget((int)recycleID1, segSize, 
			IPC_EXCL | IPC_CREAT | 0666)) != -1) {
    // attach the shared memory segment to this process
    // and store the pointer in dataPtr
    if ((dataPtr=(void *)shmat(dataID, 0, 0))==(void *) -1) {
      perror("dataPtr -- segment new operator -- attach ");
      exit(errno);
    }
  }
  else {
    perror("dataID -- segment new operator -- creation ");
    exit(errno);
  }

/*
  dataPtr = (void*)malloc(segSize);
*/

  MORON=-1;

//  cout << "Case 1: contents " << this << " dataPtr=" << this->dataPtr << " &dataPtr=" << &dataPtr << endl;
//  cout << "Segsize=" << segSize << " dataPtr=" << dataPtr << ", dataId=" << dataID << " recycleId=" << recycleID1 << endl; 
  // want to setup the semaphore

//  start_val[0] = 0;

  // create and initialize the semaphores
  if ((semid=semget(segmentID, 1, IPC_EXCL | IPC_CREAT | 0666)) != -1) {
//  if ((semid=semget(0xdeadaabe, 1, IPC_CREAT | 0666)) != -1) {
    static ushort start_val_tmp = 1;		
    arg.array = &start_val_tmp;
    if (semctl(semid, 0, SETALL, arg) == -1) {
      perror("semctl -- segment constructor -- initialization");
      exit(errno);
    }
  }
  else {
    perror("semget -- segment constructor -- creation ");
    exit(errno);
  }

  // SEG_USECOUNT is set to 0, since we only have one semaphore in this object
  incrementuse.sem_num = SEG_USECOUNT;
  incrementuse.sem_op = 1;
  incrementuse.sem_flg = 0; 
  waitfornouse.sem_num = SEG_USECOUNT;
  waitfornouse.sem_op = 0;
  waitfornouse.sem_flg = 0; 
  decrementuse.sem_num = SEG_USECOUNT;
  decrementuse.sem_op = -1;
  decrementuse.sem_flg = 0; 

  // set the table entry so that signal catcher can find it later
  // first find an empty space
  int index;

  for (index = 0; index < NUMOFSHAREDSEG; index++) {
    if (dataPtrTable[index] == NULL)
      break;
  }
  
  // more sanity check
  if (index == NUMOFSHAREDSEG) {
    perror ("ERROR: No available entry available in dataPtrTable");
    perror ("       due to implementation limitation");
    exit(-1);
  }

// Initialization
//  for(int k=0; k<segPtr->segLength; k++) {
//  	((long long int*)dataPtr)[k]=k*k;
//  }

  if (VERBOSEDEBUG || DEBUG) {
    cout << "   SEG: segment creation" << endl;
    cout << "   SEG: dataPtr - " << dataPtr << endl;     
    cout << "   SEG: segPtr - " << segPtr << endl;
    cout << "   SEG: stored in table index: " << index << endl;
  }
  dataPtrTable[index] = dataPtr;
  dataRangeTable[index] = (int *)dataPtr+segSize;
  segPtrTable[index] = segPtr;

  sim_segmentInputMask = 0;

  this_segment_is_done = 0;
  sched_this_segment_is_done = 0;

  sched_old_mode = 0;

  shouldUseUSECOUNT = 0;

  _isPage = 0;
  _isOperator = 0;
  _isSegment = 1;
//  cout << "Again Segsize=" << segSize << " dataPtr=" << dataPtr << ", dataId=" << dataID << " recycleId=" << recycleID1 << endl; 
}


///////////////////////////////////////////////////////////////////////////////
// ScoreSegment::~ScoreSegment:
//   Destructor for ScoreSegment.
//   Cleans up all internal structures.
//
// Parameters: None.
//
// Return value: None.
///////////////////////////////////////////////////////////////////////////////
ScoreSegment::~ScoreSegment() {
}


void ScoreSegment::noAccess() {
 return ;
}
/*
  // will use mprotect() call to protect a page from read/write
  // the trick is to catch the SIGSEGV signal
  // instead of exit, we want the thread/process to wait

  // increment SEG_USECOUNT
  while (semop(semid, &incrementuse, 1) == -1){
    perror("semop -- noAccess -- incrementuse ");
    if (errno != EINTR)
      exit(errno);
  }

  if (mprotect(dataPtr, segSize, PROT_NONE)) {
    perror("ERROR: Couldn't mprotect");
    exit(errno);
  }

  if (VERBOSEDEBUG || DEBUG) {
    cout << "  SEG: calling noaccess on dataPtr - " << dataPtr << endl;
  }

  shouldUseUSECOUNT = 1;
}
*/

void ScoreSegment::returnAccess() {
 return ;
}
/*
  // need to release/unprotect the memory segment and wakeup any thread/process
  // blocking on this memory segment

  if (VERBOSEDEBUG || DEBUG) {
    cout << "   SEG: calling allow returnAccess()" << endl;
  }

  if (shouldUseUSECOUNT) {
    // decrement SEG_USECOUNT
    while (semop(semid, &decrementuse, 1) == -1){
      perror("semop -- returnAccess -- decrementuse");
      if (errno != EINTR)
        exit(errno);
    }
  }

}
*/
int initSigCatch() {

  // set up signal handling
  struct sigaction sact;

  sigemptyset(&sact.sa_mask); // don't block any other signals
  sact.sa_flags = SA_SIGINFO;
  
  //  sact.sa_handler = catchSig;
  sact.sa_sigaction = catchSig;
  sigaction(SIGSEGV, &sact, (struct sigaction *) 0);

  if (VERBOSEDEBUG || DEBUG) {
    cout << "dataPtrTable initial value: " << ScoreSegment::dataPtrTable[0] 
	 << " - " << ScoreSegment::dataPtrTable[1]  << endl;
  }

  return(1);
}

void catchSig(int sig, siginfo_t *si, void *ctx) {

  int index;

  // sanity check
  if (sig == SIGSEGV) {
    // more sanity check
    if (si) {
      if (si->si_addr) {
	if (VERBOSEDEBUG || DEBUG) {
	  cout << "   CATCHSIG: caught SEGV for " << si->si_addr << endl;
	}
	// need to page align this address
	// this is also the source of the limitation of the data segment
	// if the data segment is more than one page, we possibly will not
	// find the address in dataPtrTable
	for (index = 0; index < NUMOFSHAREDSEG; index++) {
	  if ((si->si_addr >= ScoreSegment::dataPtrTable[index]) &&
	      (si->si_addr < ScoreSegment::dataRangeTable[index])) {
	    if (VERBOSEDEBUG || DEBUG) {
	      cout << "   CATCHSIG: found segfault address in index: "
		   << index << endl;
	    }
	    break;
	  }
	}
	
	// if we cannot find this address in the dataPtrTable,
	// it means it is a real segfault, and we exit
	if (index == NUMOFSHAREDSEG) {
	  cerr << "   CATCHSIG: This is a real segfault: " << si->si_addr <<
	    endl;
	  exit(-1);
	}

	// else we usr the index to find the ScoreSegment object pointer
	ScoreSegment *obj = ScoreSegment::segPtrTable[index];

	if (VERBOSEDEBUG) {
	  cout << "   CATCHSIG: semaphoreID is - " << obj->semid << endl;
	}

	// try to test the semaphore
	if (VERBOSEDEBUG) {
	  cout << "   SEG: the semop for waitfornouse is: " << 
	    obj->waitfornouse.sem_op << endl;
	}
	while (semop(obj->semid, &(obj->waitfornouse), 1) == -1){
	  perror("semop -- catchSig -- waitfornouse ");
	  if (errno != EINTR)
	    exit(errno);
	}

	if (VERBOSEDEBUG || DEBUG) {
	  cout << "   CATCHSIG: waking up" << endl;
	}

	mprotect(ScoreSegment::dataPtrTable[index], 
	  obj->segSize, PROT_READ|PROT_WRITE);
	return;
      } else {
	printf("seg fault address is NULL\n");
	exit(-1);
      }
    }else
      cout << "caught SEGV, no siginfo!" << endl;
  } else
    printf("caught unexpected sig \n");
  exit(1);
}


char ScoreSegment::checkIfAddrFault(unsigned int newAddr) {
  return(!((newAddr >= traAddr) && 
	   (newAddr < (traAddr+maxAddr))));
}

SCORE_SEGMENT_ID segmentOBJ_to_ID(ScoreSegment *seg) {
  // take access away from this process.
  seg->noAccess();

  return(seg->id());
}

ScoreSegment *segmentID_to_OBJ(int nid) {

  if (VERBOSEDEBUG || DEBUG) {
    cout << "   SEG: segmentID received - " << nid << endl;
  }
   
  ScoreSegment *segPtr;

  while ((segPtr=(ScoreSegment *)shmat(nid, 0, 0))==(ScoreSegment *) -1) {
    perror("segPtr -- SegmentID_to_OBJ -- attach ");
    if (errno != EINTR)
      exit(errno);
  }
  return(segPtr);
}


// required by LEDA for use with lists/etc.

// provides comparison operation between ScoreStreams.
// NOTE: Right now, we only say 2 streams are equal if their pointers
//       are equal. Otherwise, less than/greater than is determined
//       simply by their pointer values.
int compare(ScoreSegment * const & left, ScoreSegment * const & right) {
  if (left == right) {
    return(0);
  } else if (left < right) {
    return(-1);
  } else {
    return(1);
  }
}
