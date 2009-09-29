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
// $Revision: 2.100 $
//
//////////////////////////////////////////////////////////////////////////////

#define STREAM_MAIN_FILE

#include <sched.h>
#include <errno.h>
#include <semaphore.h>
//#include <asm/msr.h>
#include "ScoreGraphNode.h"
#include "ScoreStream.h"
#include "ScoreCustomStack.h"
#include "ScoreConfig.h"
#include "ScoreStreamStitch.h"
#include <iostream> // added by Nachiket for newer file IO constructs
#include <iomanip> // added by Nachiket for newer file IO constructs
#include <ios> // added by Nachiket for newer file IO constructs

#include "ScorePage.h"

using std::ios;
using std::fstream;
using std::setfill;
using std::setw;

#if 0
#define PRINT_MSG(_msg_)     fprintf(stderr, "%d: %s: %s\n", __LINE__, __FUNCTION__, # _msg_)
#else
#define PRINT_MSG(_msg_)
#endif


int ScoreStream::doneSemId = -1;

#if 1
#define PRINT_SEM(__op__,__ptr__) \
fprintf(stderr, "%d: %s: %s stream[%u] type[%x]\n", __LINE__, __FUNCTION__, # __op__, (unsigned int) __ptr__, __ptr__ ? (*(AllocationTag*)__ptr__) : -1);
#else 
#define PRINT_SEM(__op__,__ptr__)
#endif

int ScoreStream::currentID = 0;
int ScoreStream::tempID = 0;


void *ScoreStream::operator new(size_t size, AllocationTag allocTag) {

  void *ptr = 0;

/* idiotic code switch
  // need to get new streamID
  int file = (int) open("/tmp/streamid",O_RDONLY);
  
  if (file < 0) {
    cout << "Initialize /tmp/streamid" << endl;
    close(file);
    // initialize the file..
    fstream outfile("/tmp/streamid",ios::out);
    outfile << setfill('0');
    outfile << setw(4);
    outfile << 0;
    outfile.close();
  } else {
    int rdCount = read(file, &tempID, 4);
    if(rdCount!=4) {
      cout << "Got merely " << rdCount << "bytes" << endl;
      perror("ERROR: Did not get 1 bytes of data back");
      exit(errno);
    }
    cout << "Found existing /tmp/streamid: " << tempID << endl;
    close(file);
  }
*/


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
	  char* buffer=new char[4];
	  infile.read(buffer,4);
	  tempID=(buffer[3]-48)+
		  10*(buffer[2]-48)+
		  100*(buffer[1]-48)+
		  1000*(buffer[0]-48);
	  cout << "Found existing /tmp/streamid: " << tempID << endl;
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
  fstream outfile("/tmp/streamid",ios::out);
  outfile << setfill('0');
  outfile << setw(4);
  outfile << tempID+1;
  outfile.close();

  if(errno) {
  	perror("Something went wrong..");
	exit(errno);
  }

  size_t allocatedSize = 0;

  if (allocTag == ALLOCTAG_SHARED) {
    PRINT_MSG(ALLOCTAG_SHARED);
    // create a new shared segment and malloc
    // need to allocate new segment of shared memory
    
    // allocate space for extra tokens
    size += sizeof(ScoreToken) * (DEFAULT_N_SLOTS - ARRAY_FIFO_SIZE);
    if ((currentID=shmget((int)tempID, size + sizeof(AllocationTag),
			  IPC_EXCL | IPC_CREAT | 0666))
	!= -1) {
      if ((ptr = shmat(currentID, 0, 0)) == (ScoreStream *) -1) {
	perror("shmptr -- stream new operator -- attach ");
	exit(errno);
      }
    } else {
      cout << currentID << endl;
      cout << tempID << endl;
      perror("currentID -- stream new operator -- creation ");
      exit(errno);
    }

    allocatedSize = size + sizeof(AllocationTag);

    if (VERBOSEDEBUG || DEBUG) {
      cout << "   StreamID is: " << currentID << "\n";
    }
  } else if (allocTag == ALLOCTAG_PRIVATE) {
    PRINT_MSG(ALLOCTAG_PRIVATE);
    errno = 0;
    ptr = malloc(size + sizeof(AllocationTag));
    if (!ptr) {
      perror("ptr -- stream new operator -- unable to malloc mem");
      exit(errno);
    }
    currentID = tempID;

    allocatedSize = size + sizeof(AllocationTag);

  } else { // allocTag did not match predefined values
    fprintf(stderr, "invalid allocation tag %x\n", allocTag);
    exit(1);
  }

  assert(ptr);

  // clean the allocated area
  assert(allocatedSize > 0);
  //memset(ptr, 0, allocatedSize);

  // 
  *((AllocationTag*)ptr) = allocTag;

  // add the allocation tag at the end of the alloted block
  *((AllocationTag*)(((char*)ptr) + allocatedSize - sizeof(AllocationTag))) =
    allocTag;

  return ptr;
}
  
void ScoreStream::operator delete(void *rawMem, size_t size)
{
  // get the allocation tag
  AllocationTag allocTag_copy1 = *((AllocationTag*)rawMem);

  ScoreStream *strm = (ScoreStream *)rawMem;
  
  strm->arg.val = 0;
  int tmp0 = strm->streamID;
    
  strm->memoized_runtimePtr = NULL;

  if (allocTag_copy1 == ALLOCTAG_SHARED) {
    PRINT_MSG(ALLOCTAG_SHARED);
    ScoreStream *strm = (ScoreStream *)rawMem;

    AllocationTag allocTag = 
      *((AllocationTag*)(((char*)rawMem) + sizeof(ScoreStream) + sizeof(ScoreToken) * (DEFAULT_N_SLOTS - ARRAY_FIFO_SIZE)));

    assert(allocTag == allocTag_copy1);
    
    shmdt((char *)strm); // detach memory segment
    shmctl(tmp0,IPC_RMID,(struct shmid_ds *)0); // remove memory segment
  
  } else if (allocTag_copy1 == ALLOCTAG_PRIVATE) {
    PRINT_MSG(ALLOCTAG_PRIVATE);

    AllocationTag allocTag = *((AllocationTag*)(((ScoreStream*)rawMem) + 1));

    assert(allocTag == allocTag_copy1);

    free(rawMem);
 
  } else {
    fprintf(stderr, "unknown allocTag = %x\n", allocTag_copy1);
    exit(1);
  } 
}


ScoreStream::ScoreStream() {}


ScoreStream::ScoreStream(int width_t, int fixed_t, int length_t,
				       ScoreType type_t,
				       unsigned int user_stream_type,
				       int depth_hint_t) {


#ifndef NDEBUG
  // check length_t is not greater than was allocated
  if (tag_copy1 == ALLOCTAG_SHARED) {
    assert(length_t == DEFAULT_N_SLOTS);

    AllocationTag last_tag = *((AllocationTag*)
			       (buffer + DEFAULT_N_SLOTS + 1 + 1));
    assert(tag_copy1 == last_tag);

  } else if (tag_copy1 == ALLOCTAG_PRIVATE) {
//  cout << length_t << "," << ARRAY_FIFO_SIZE << endl;
    assert(length_t == ARRAY_FIFO_SIZE);

    AllocationTag last_tag = *((AllocationTag*)
			       (buffer + ARRAY_FIFO_SIZE + 1 + 1));
    assert(tag_copy1 == last_tag);

  } else {
    fprintf(stderr, "ERROR: %s: %d: unknown allocation tag = %x\n", 
	    __FILE__, __LINE__, tag_copy1);
    assert(0);
  }
#endif  

  // constructor for ScoreStream class
  width = width_t;
  fixed = fixed_t;

  depth_hint = depth_hint_t;
  // NOTE: currently each stream is created with the buffer size
  //       DEFAULT_N_SLOTS. If this changes, the following code
  //cerr << "depth_hint = " << depth_hint << endl;
  //cerr << "length_t = " << length_t << endl;
  
  if (depth_hint > length_t) {
    //    cerr << "appending to the streamsToExpand\n";
    // after a graph is created, scheduler will insert
    // a stitch buffer on each of the streams in this list
    if (!streamsToExpand) { cerr << "streamsToExpand did not exist\n"; 
	    	streamsToExpand = new list<ScoreStream*>; }
    streamsToExpand->append(this);
  }


  streamID = currentID;
  recycleID = tempID;
  start_val[0] = length_t+1;
  start_val[1] = 0;
  start_val[2] = 1;
//	cout << "length_t="  << length_t << " store as " << start_val[0] << endl;
//	exit(-1);

  head = tail = 0; 
  token_written = token_read = token_eos = 0;
  src = sink = NULL;
  sched_src = sched_sink = NULL;

  if (USE_POLLING_STREAMS) {
    
//    cout << "Semaphore status:" << ScoreStream::doneSemId << endl;
    // one shared semaphore needs to be initialized
    if (doneSemId == -1) {
      if ((doneSemId = semget(0xfeedbabe, 1, IPC_CREAT | 0666))
	  == -1) {
	perror("doneSemId -- semget -- creation ");
	exit(errno);
      }
      /*
      else {
		// copied by Nachiket from ScoreRuntime.cc
	      if (semctl(doneSemId, 0, SETALL, arg) == -1) {
		      perror("gDoneSemId -- semctl -- initialization");
		      exit(errno);
	      }
      }
      */
    }
  } else {
    // create and initialize the semaphores
    if ((semid=semget(streamID, 3, IPC_EXCL | IPC_CREAT | 0666)) != -1) {
      arg.array = start_val;
      if (semctl(semid, 0, SETALL, arg) == -1) {
	perror("semctl -- stream constructor -- initialization");
	exit(errno);
      }
    }
    else {
      perror("semget -- stream constructor -- creation ");
      exit(errno);
    }
//    cout << "Semaphore status:" << semid << " for streamID=" << streamID  << endl;
  }
  
  acquire.sem_num = 0;
  acquire.sem_op = -1;
  acquire.sem_flg = 0; // SEM_UNDO;
  release.sem_num = 0;
  release.sem_op = 1;
  release.sem_flg = 0; // SEM_UNDO;

  interProcess = 0;
  producerClosed = 0;
  consumerFreed = 0;
  producerClosed_hw = 0;
  consumerFreed_hw = 0;

  srcIsDone = 0;
  sinkIsDone = 0;
  sched_srcIsDone = 0;
  sched_sinkIsDone = 0;

  srcFunc = STREAM_OPERATOR_TYPE;
  snkFunc = STREAM_OPERATOR_TYPE;
  sched_srcFunc = STREAM_OPERATOR_TYPE;
  sched_snkFunc = STREAM_OPERATOR_TYPE;

  length = length_t;

  isCrossCluster = 0;
  sched_isCrossCluster = 0;

  type = type_t;

  sched_isStitch = 0;
  sched_isPotentiallyFull = 0;
  sched_isPotentiallyEmpty = 0;

  sched_spareStreamStitchList = NULL;

  sched_isProcessorArrayStream = 0;

  if (user_stream_type == USER_READ_STREAM) {
    readThreadCounter = globalCounter->threadCounter;
    writeThreadCounter = NULL;
    threadCounterPtr = NULL;
  } else if (user_stream_type == USER_WRITE_STREAM) {
    readThreadCounter = NULL;
    writeThreadCounter = globalCounter->threadCounter;
    threadCounterPtr = NULL;
  } else {
    readThreadCounter = NULL;
    writeThreadCounter = NULL;
    threadCounterPtr = NULL;
  }

  realTimeRead[0] = threadTimeRead[0] = 0;
  realTimeWrite[0] = threadTimeWrite[0] = 0;

#if 0
  for (int i=0; i < DEFAULT_N_SLOTS+1+1; i++) {
    buffer[i].token = 0;
    buffer[i].timeStamp = 0;
  }
#endif

  threadCounterPtr = NULL;

  sim_sinkOnStallQueue = 0;
  sim_haveCheckedSinkUnstallTime = 0;

  memoized_runtimePtr = NULL;

}

ScoreStream::~ScoreStream()
{
  if (VERBOSEDEBUG || DEBUG) {
    cerr << "[SID=" << streamID << "][" << (unsigned int) this << "] "
	 << "ScoreStream Destructor called on " << streamID << endl;
  }
  
  if (!USE_POLLING_STREAMS) {
    union semun arg;
    semctl(semid,0,IPC_RMID,arg);
  }

#if 0
  // GC old stream ID by writing 4 bytes to FILE streamid
     
  int file = (int) open("/proc/streamid",O_WRONLY);
     
  if (file < 0) {
    perror("ERROR: Could not open /proc/streamid ");
    exit(errno);
  }
     
  if (write(file, &(recycleID), 4) != 4) {
    perror("ERROR: Did not recycle 4 bytes of data back ");
    exit(errno);
  }
     
  close(file);

  if (VERBOSEDEBUG || DEBUG) {
    cout << "[SID=" << tmp0 << "]  "
         << "   Recycle StreamID: " << recycleID << endl;
  }
#endif

}

long long int ScoreStream::stream_read_array() {

  if (consumerFreed) {
    cerr << "SCORESTREAMERR: ATTEMPTING TO READ FROM A FREED STREAM!" 
	 << endl;
    exit(1);
  }

  read_local_buffer = buffer[head].token;

  head = (head+1) % (length+1+1);
  token_read++;

  // increment the sink node's input consumption.
  if (snkFunc != STREAM_OPERATOR_TYPE) {
    sink->incrementInputConsumption(snkNum);
  }

  return read_local_buffer;
};

long long int ScoreStream::stream_read(long long unsigned _cTime) {

  long long unsigned cTime;

  if (readThreadCounter != NULL) {
    cTime = _cTime - 
      readThreadCounter->record->totalStreamOverhead;
  } else if (threadCounterPtr != NULL) {
    cTime = threadCounterPtr->record->simCycle - 
      threadCounterPtr->record->simStartCycle;
  } else {
    cTime = 0;
  }

  long long int local_buffer;

  if (VERBOSEDEBUG || DEBUG || VERBOSE_STREAM)
    cout << "[SID=" << streamID << "][" << (unsigned int) this << "] "
	 << " producerClosed = " << producerClosed
	 << "   Entering stream_read " << streamID 
	 << " interProcess=" << interProcess
	 << endl;

  // if stream is freed, disallow any more reads.
  if (consumerFreed) {
    cerr << "SCORESTREAMERR: ATTEMPTING TO READ FROM A FREED STREAM!" << endl;
    exit(1);
  }

  if (USE_POLLING_STREAMS) {
    // Nachifix: Why is this thread quitting?
    while (head == tail) {
      sched_yield();
    }
  } else {
    if (!(sched_isStitch)) {
      acquire.sem_num = TO_CONSUME;

      union semun arg;
      int value=semctl(semid, 0, GETVAL, arg);
    
//      cout << "Acquiring semaphore for read=" << semid << " value=" << value << endl;
      while (semop(semid, &acquire, 1) == -1) {
	perror("semop -- stream_read -- acquire ");
	if (errno != EINTR)
	  exit(errno);
      }
    } else {
      while (sem_wait(&sem_TO_CONSUME) == -1){
	perror("sem_wait -- stream_read -- TO_CONSUME ");
	if (errno != EINTR)
	  exit(errno);
      }
    }
  }

  local_buffer = buffer[head].token;
  if (VERBOSEDEBUG || DEBUG) {
    printf("[SID=%d]   Stream Read: %llx [%llu] [%llu]\n", 
	   streamID, local_buffer, cTime, buffer[head].timeStamp);

  }
  head = (head+1) % (length+1+1);
  token_read++;

  // increment the sink node's input consumption.
  if (snkFunc != STREAM_OPERATOR_TYPE) {
    sink->incrementInputConsumption(snkNum);
  }

  if (VERBOSEDEBUG || DEBUG) {
    if (local_buffer == (long long int)EOS) {
      cout << "[SID=" << streamID << "]  "
	   << "   stream_read error: read EOS token" << endl;
      cerr << "EOS during read" << endl;
      exit(1);
    }
  }

  if (!USE_POLLING_STREAMS) {
    if (!(sched_isStitch)) {
      release.sem_num = AVAIL_SLOTS;
    
      union semun arg;
      int value = semctl(semid, 0, GETVAL, arg);

//      cout << "Releasing semaphore for read=" << semid  << " before_value=" << value << endl;
      while (semop(semid, &release, 1) == -1){
	perror("semop -- stream_read -- release ");
	if (errno != EINTR)
	  exit(errno);
      }
      value = semctl(semid, 0, GETVAL, arg);
//      cout << "Releasing semaphore for read=" << semid  << " after_value=" << value << endl;
    } else {
      while (sem_post(&sem_AVAIL_SLOTS) == -1){
	perror("sem_post -- stream_read -- AVAIL_SLOTS ");
	if (errno != EINTR)
	  exit(errno);
      }    
    }
  }

  return local_buffer;
}

void ScoreStream::stream_write_array(long long int input,int writingEOS) {

  if (VERBOSEDEBUG || DEBUG || VERBOSE_STREAM)
    cout << "[SID=" << streamID << "][" << (unsigned int) this << "] "
	 << " producerClosed = " << producerClosed
	 << "   Entering stream_write_array(" <<input << ", " << writingEOS
	 << ")" << endl;


  // if the producerClosed, then disallow any more writes!
  if (producerClosed && !writingEOS) {
    cerr << "SCORESTREAMERR: ATTEMPTING TO WRITE TO A CLOSED STREAM! -- line "
	 << __LINE__ << " [SID=" << streamID << "][" << (unsigned int)this 
	 << "] " << (sched_isStitch ? "STITCH" : "NOT")
	 << " producerClosed = " << producerClosed << endl;
    assert(0);
    exit(1);
  }

  // if the sinkIsDone, then just complete since all tokens are being thrown
  // away!
  if (sinkIsDone) {
    return;
  }

  if (VERBOSE_STREAM) {
    cerr << "stream_write_array -- line " << __LINE__ << " [SID=" << streamID
	 << "][" << (unsigned int)this 
	 << "] " << (sched_isStitch ? "STITCH" : "NOT")
	 << " producerClosed = " << producerClosed
	 << " tail = " << tail << " length = " << length << endl; 
  }

  buffer[tail].token = input;
  tail = (tail+1) % (length+1+1);
  token_written++;

  // increment the source node's output production.
  if (srcFunc != STREAM_OPERATOR_TYPE) {
    src->incrementOutputProduction(srcNum);
  }
}


void ScoreStream::stream_write(long long int input, int writingEOS, 
			       long long unsigned _cTime) {

  /*
    0. is the sink page resident (not -1)?
    1. is the sink page on stallQ (check flag)?
    2. is this the input the sink page is waiting for (check state flag)?
    3. use the resident flag to index the checkQ
    4. update the checkQ and set the "dirty" flag
   */

  if (VERBOSEDEBUG || DEBUG || VERBOSE_STREAM)
    cout << "[SID=" << streamID << "][" << (unsigned int) this << "] "
	 << " producerClosed = " << producerClosed
	 << "   Entering stream_write(" << input << ", " << writingEOS
	 << ")" << endl;

  long long unsigned cTime;

  if (writingEOS) {
    cTime = 0;
  } else if (writeThreadCounter != NULL) {
    cTime = _cTime - writeThreadCounter->record->totalStreamOverhead;
    if (DEBUG) {
      printf("[SID=%d]   Stream Write _cTime         %llu\n", streamID,_cTime);
      printf("[SID=%d]   Stream Write streamOverhead %llu\n", streamID,
	     writeThreadCounter->record->totalStreamOverhead);
    }
  } else if (threadCounterPtr != NULL) {
    // should be simcycle - simStartCycle
    cTime = threadCounterPtr->record->simCycle -
      threadCounterPtr->record->simStartCycle;
  } else {
    cTime = 0;
  }

  // if the producerClosed, then disallow any more writes!
  if (producerClosed && !writingEOS) {
    cerr << "SCORESTREAMERR: ATTEMPTING TO WRITE TO A CLOSED STREAM! -- line "
	 << __LINE__ << " [SID=" << streamID << "][" << (unsigned int)this 
	 << "] " << (sched_isStitch ? "STITCH" : "NOT")
	 << " producerClosed = " << producerClosed << endl;
    assert(0);
    exit(1);
  }

  // if the sinkIsDone, then just complete since all tokens are being thrown
  // away!
  if (sinkIsDone) {
    if (VERBOSEDEBUG || DEBUG) {
      cout << "[SID=" << streamID << "]  "
           << "Throwing away written token since sinkIsDone!" << endl;
    }

    return;
  }

  if (USE_POLLING_STREAMS) {
    while (((head - tail) == 1) || 
	   ((tail == ((length+1+1)-1) && (head == 0)))) {
      sched_yield();
    }
  } else {
    if (!(sched_isStitch)) {
      acquire.sem_num = AVAIL_SLOTS;
    
      union semun arg;
      int value = semctl(semid, 0, GETVAL, arg);

//      cout << "Acquiring semaphore for write=" << semid << " value=" << value << endl;
      while (semop(semid, &acquire, 1) == -1){
	perror("semop -- stream_write -- acquire ");
	if (errno != EINTR)
	  exit(errno);
      }
    } else {
      while (sem_wait(&sem_AVAIL_SLOTS) == -1){
	perror("sem_wait -- stream_write -- AVAIL_SLOTS ");
	if (errno != EINTR)
	  exit(errno);
      }
    }
  }

  // mask the read data to make sure it falls within the specified width.
#if 0
  if ((width < 64) && (writingEOS != 1)) {
    long long int bitMask = (1 << width) - 1;

    input = input & bitMask;
  }
#endif

  if (VERBOSE_STREAM) {
    cerr << "stream_write_array -- line " << __LINE__ << " [SID=" << streamID
	 << "][" << (unsigned int)this 
	 << "] " << (sched_isStitch ? "STITCH" : "NOT")
	 << " producerClosed = " << producerClosed
	 << " tail = " << tail << " length = " << length << endl; 
  }

  buffer[tail].token = input;
  buffer[tail].timeStamp = cTime;
  tail = (tail+1) % (length+1+1);
  token_written++;
  if (VERBOSEDEBUG || DEBUG) {
    printf("[SID=%d]   Stream Write: %llx [%llu].\n", streamID,input,cTime);
  }

  // NOTE: I choose not to mask the write data since we could use it later for
  //       debugging that data is too large for the stream width. Also, this allows
  //       the EOS token to get written unhindered. Bit masking is done when
  //       

  // increment the source node's output production.
  if (srcFunc != STREAM_OPERATOR_TYPE) {
    src->incrementOutputProduction(srcNum);
  }

  if (!USE_POLLING_STREAMS) {
    if (!(sched_isStitch)) {
      release.sem_num = TO_CONSUME;
    
      union semun arg;
      int value = semctl(semid, 0, GETVAL, arg);

//      cout << "Releasing semaphore for write=" << semid << " value=" << value << endl;
      while (semop(semid, &release, 1) == -1){
	perror("semop -- stream_write -- release ");
	if (errno != EINTR)
	  exit(errno);
      }
    } else {
      while (sem_post(&sem_TO_CONSUME) == -1){
	perror("sem_post -- stream_write -- TO_CONSUME ");
	if (errno != EINTR)
	  exit(errno);
      }
    }
  }

  // see if we need to recheck any stalled pages/segments in the simulator.
  if (sim_sinkOnStallQueue) {
    if (!(sim_haveCheckedSinkUnstallTime)) {
      sim_haveCheckedSinkUnstallTime = 1;

      // FIX ME! SHOULD NOW PUT IN CODE TO UPDATE THE GLOBAL CHECK NODE
      //         LIST!
    }
  }
}


int ScoreStream::stream_eos() {

  int isEOS = 0;

  token_eos++;

  // make sure there is a token in the stream
  // and exam it to see if it is a EOS token
  // otherwise block

  long long int local_buffer;

  if (VERBOSEDEBUG)
    cout << "[SID=" << streamID << "]  "
	 << "   Entering stream_eos \n";

  if (USE_POLLING_STREAMS) {
    while (head == tail) {
      sched_yield();
    }
  } else {
    if (!(sched_isStitch)) {
      acquire.sem_num = TO_CONSUME;
    
      while (semop(semid, &acquire, 1) == -1){
	perror("semop -- stream_read -- acquire ");
	if (errno != EINTR)
	  exit(errno);
      }
    } else {
      while (sem_wait(&sem_TO_CONSUME) == -1){
	perror("sem_wait -- stream_read -- TO_CONSUME ");
	if (errno != EINTR)
	  exit(errno);
      }
    }
  }

  // if the produce has closed the stream and there is only 1 more token
  // left in the stream, then it must be EOS!
  if (producerClosed && (get_numtokens() == 1)) {
    local_buffer = buffer[head].token;
    
    // check the token value to make sure it is the EOS token.
    // NOTE: THE REASON FOR THIS CHECK IS THAT, SINCE INSERTING THE
    //       EOS TOKEN AND SETTING THE PRODUCERCLOSED FLAG IS NOT ATOMIC,
    //       THEN THIS IS A POSSIBLE CONDITION!
    if (local_buffer == (long long int)EOS) {
      isEOS = 1;
      
      if (snkFunc != STREAM_OPERATOR_TYPE) {
	sink->incrementInputConsumption(snkNum);
      }
    } else {
      isEOS = 0;
    }
  }


  if (!USE_POLLING_STREAMS) {
    if (!(sched_isStitch)) {
      release.sem_num = TO_CONSUME;
      
      while (semop(semid, &release, 1) == -1){
	perror("semop -- stream_write -- release ");
	if (errno != EINTR)
	  exit(errno);
      }
    } else {
      while (sem_post(&sem_TO_CONSUME) == -1){
	perror("sem_post -- stream_write -- TO_CONSUME ");
	if (errno != EINTR)
	  exit(errno);
      }
   }
  }

  return(isEOS);
}


int ScoreStream::stream_data() {

  // if the head pointer does not equal to the tail pointer,
  // then there is data in the stream and return a non-zero number
#if 0
  if (threadCounterPtr != NULL) {
    return ((head != tail) && 
	    (buffer[head].timeStamp <= threadCounterPtr->simCycle));
  } else {
#endif
    return (head != tail);
#if 0
  }
#endif
}


// checks whether there are ANY tokens (present/future) in the stream.

int ScoreStream::stream_data_any() {

  // if the head pointer does not equal to the tail pointer,
  // then there is data in the stream and return a non-zero number
  return (head != tail);
}


int ScoreStream::get_numtokens() {
  // FIX ME! THIS MAY NOT BE SAFE! (NO LOCKING!)
  int numTokens = tail-head;

  if (numTokens < 0) {
    return(numTokens+length+1+1);
  } else {
    return(numTokens);
  }
}


void ScoreStream::set_length(int length_t) {

  // need to do more than change the length variable
  // also need to reset semaphore for this to work right

  length = length_t;
}


// Returns whether or not this stream is GCable.
//
// To summarize:
// 
// Given:
//         ScoreStream myStream;
// 
// Available actions:
//         STREAM_CLOSE()
//         STREAM_FREE()
//         STREAM_CLOSE_HARDWARE()
//         STREAM_FREE_HARDWARE()
// 
// isSrcOnArray = (myStream->srcFunc == STREAM_PAGE_TYPE) ||
//                (myStream->srcFunc == STREAM_SEGMENT_TYPE)
// isSinkOnArray = (myStream->snkFunc == STREAM_PAGE_TYPE) ||
//                 (myStream->snkFunc == STREAM_SEGMENT_TYPE)
// 
// if myStream processor-processor (!isSrcOnArray && !isSinkOnArray), then
//   GC stream when have seen STREAM_CLOSE on src and STREAM_FREE on sink.
// 
// if myStream processor-array (!isSrcOnArray && isSinkOnArray), then
//   GC stream when have seen STREAM_CLOSE on src and STREAM_FREE_HARDWARE on
//   sink.
// 
// if myStream array-processor (isSrcOnArray && !isSinkOnArray), then
//   GC stream when have seen STREAM_CLOSE_HARDWARE on src and STREAM_FREE on
//   sink.
// 
// if myStream array-array (isSrcOnArray && isSinkOnArray), then
//   GC stream when have seen STREAM_CLOSE_HARDWARE on src and
//   STREAM_FREE_HARDWARE on sink.
char ScoreStream::isGCable() {
  char isSrcOnArray = ((srcFunc == STREAM_PAGE_TYPE) ||
		       (srcFunc == STREAM_SEGMENT_TYPE));
  char isSinkOnArray = ((snkFunc == STREAM_PAGE_TYPE) ||
			(snkFunc == STREAM_SEGMENT_TYPE));


  if (!isSrcOnArray && !isSinkOnArray) {
    return(producerClosed && consumerFreed);
  }
  if (!isSrcOnArray && isSinkOnArray) {
    return(producerClosed && consumerFreed_hw);
  }
  if (isSrcOnArray && !isSinkOnArray) {
    return(producerClosed_hw && consumerFreed);
  }
  if (isSrcOnArray && isSinkOnArray) {
    return(producerClosed_hw && consumerFreed_hw);
  }

  return(0);
}


// this returns the scheduled maturation time of the head token in the
// stream.
unsigned long long ScoreStream::stream_head_futuretime() {
  if (head == tail) {
    cerr << "SCORESTREAMERR: TRYING TO CHECK THE FUTURE TIME OF A " <<
      "NON-EXISTANT HEAD TOKEN!" << endl;
    exit(1);
  }

  return(buffer[head].timeStamp);
}


void stream_close(ScoreStream *strm) {

  if (VERBOSE_STREAM) 
    cerr << "[SID=" << strm->streamID << "]   entering stream_close" << endl;

  // make sure this is not a double close.
  if (strm->producerClosed) {
    cerr << "SCORESTREAMERR: TRYING TO DOUBLE CLOSE A STREAM! " << 
      (unsigned int) strm << endl;
    exit(1);
  }

  if (TIMEACC) {
    if (strm->writeThreadCounter != NULL) {
      strm->writeThreadCounter->record->numOfUserStreamOPs +=
	strm->get_stream_tokens_written();
      strm->writeThreadCounter->record->numOfStreamWriteOPs += 
	strm->get_stream_tokens_written();
      strm->writeThreadCounter->record->streamThreadTime +=
	strm->threadTimeWrite[0];
    } else if (strm->readThreadCounter == NULL) {
      // the reason for the else clause
      // this is to get around an unknown bug
      // where the ptr gets reset to some unknown value
      ScoreThreadCounter *ptr;
      ptr = strm->getThreadCounterPtr();
      if (strm->getThreadCounterPtr() != NULL) {
	strm->getThreadCounterPtr()->record->numOfStreamWriteOPs += strm->get_stream_tokens_written();
      } else {
	cerr << "ERROR: stream_close - ptr is NULL" << endl;
	exit(-1);
      }
    }
  }

  if (!(strm->sched_isStitch)) {
    if (VERBOSEDEBUG || DEBUG || VERBOSE_STREAM) {
      cerr << "[SID=" << strm->streamID << "]   stream_close called" << endl;
    }
    
    if (USE_POLLING_STREAMS) {
      strm->acquire.sem_num = 0;
      PRINT_SEM(b_acquire,strm);
      // Nachiket's fix... WTF is the deal with these semaphores?
      cout << "Attemping to access semaphore id=" << ScoreStream::doneSemId << endl;
      while(semop(ScoreStream::doneSemId, &(strm->acquire), 1) == -1) {
	perror("semop -- stream_close -- acquire");
	if (errno != EINTR)
	  exit(errno);
      }
      PRINT_SEM(acquire,strm);
    } else {
      // want to acquire the mutex DONE_MUTEX first
      
      strm->acquire.sem_num = DONE_MUTEX;
      
      while (semop(strm->semid, &(strm->acquire), 1) == -1){
	perror("semop -- stream_close -- acquire ");
	if (errno != EINTR)
	  exit(errno);
      }
    }
    
    if (VERBOSEDEBUG || DEBUG || VERBOSE_STREAM) {
      cerr << "[SID=" << 
	strm->streamID << "]   stream_close mutex acquired" << endl;
    }
    
    strm->producerClosed = 1;
    
    // need to write a EOS token to the stream
    // strm->stream_write(EOS, 1);
    
    if (strm->isGCable()) { // recycle everything
      if (VERBOSEDEBUG || DEBUG || VERBOSE_STREAM) {
	cerr << "[SID=" << strm->streamID << "]   stream_close GCing stream" <<
	  endl;
      }
      
      stream_gc(strm);

      if (USE_POLLING_STREAMS) {
	struct sembuf release;
	release.sem_num = 0;
	release.sem_op = 1;
	release.sem_flg = 0; // SEM_UNDO;
	PRINT_SEM(b_release,0);
	while (semop(ScoreStream::doneSemId, &release, 1) == -1) {
	  perror("semop -- stream_close -- release ");
	  if (errno != EINTR)
	    exit(errno);
	}
	PRINT_SEM(release,0);
      }
    } else {
      char shouldDetach = 0;
      
      if (strm->srcFunc == STREAM_OPERATOR_TYPE) {
	if (strm->interProcess) {
	  shouldDetach = 1;
	}
      }
      
      if (VERBOSEDEBUG || DEBUG || VERBOSE_STREAM) {
	cerr << 
	  "[SID=" << strm->streamID << "]   stream_close is not GCing " <<
	  "stream" << endl;
      }
      
      if (USE_POLLING_STREAMS) {
	strm->release.sem_num = 0;
	PRINT_SEM(b_release,strm);
	while (semop(ScoreStream::doneSemId, &(strm->release), 1) == -1) {
	  perror("semop -- stream_close -- release ");
	  if (errno != EINTR)
	    exit(errno);
	}
	PRINT_SEM(release,strm);
      } else {
	// need to release the semaphore
	strm->release.sem_num = DONE_MUTEX;
	while (semop(strm->semid, &(strm->release), 1) == -1){
	  perror("semop -- stream_close -- release ");
	  if (errno != EINTR)
	    exit(errno);
	}
      }

      // need to write a EOS token to the stream
      strm->stream_write(EOS, 1);
      
      // if the source is an operator, then we will not get a
      // stream_close_hw later on, so detach the segment now.
      if (shouldDetach) {
        strm->memoized_runtimePtr = NULL;
	shmdt((char *)strm); // detach memory segment
      }
    }
  } else {
    if (VERBOSEDEBUG || DEBUG || VERBOSE_STREAM) {
      cerr << "[SID=" << strm->streamID << "]   stream_close called" << endl;
    }
    
    if (USE_POLLING_STREAMS) {
      strm->acquire.sem_num = 0;
      PRINT_SEM(b_acquire,strm);
      while(semop(ScoreStream::doneSemId, &(strm->acquire), 1) == -1) {
	perror("semop -- stream_close -- acquire");
	if (errno != EINTR)
	  exit(errno);
      }
      PRINT_SEM(acquire,strm);
    } else {
      while (sem_wait(&(strm->sem_DONE_MUTEX)) == -1){
	perror("sem_wait -- stream_close -- DONE_MUTEX ");
	if (errno != EINTR)
	  exit(errno);
      }
    }

    if (VERBOSE_STREAM) {
      cerr << "[SID=" << strm->streamID << "]  doneSemId acquired\n";
    }

    strm->producerClosed = 1;
    
    // need to write a EOS token to the stream
    //strm->stream_write(EOS, 1);

    if (strm->isGCable()) { // recycle everything
      // have to do this here since this is the only time we know if we
      // can truly GC this stream!
      ScoreStreamStitch *strmStitch = (ScoreStreamStitch *) strm;

      strmStitch->reset();
      SCORECUSTOMSTACK_PUSH(strm->sched_spareStreamStitchList, strm);
    } else {
      if (!USE_POLLING_STREAMS) {
	while (sem_post(&(strm->sem_DONE_MUTEX)) == -1){
	  perror("sem_post -- stream_close -- DONE_MUTEX ");
	  if (errno != EINTR)
	    exit(errno);
	}
      }
    }
    
    if (USE_POLLING_STREAMS) {
      strm->release.sem_num = 0;
      PRINT_SEM(b_release,strm);
      while (semop(ScoreStream::doneSemId, &(strm->release), 1) == -1) {
	perror("semop -- stream_close -- release ");
	if (errno != EINTR)
	  exit(errno);
      }
      PRINT_SEM(release,strm);
    }

    // need to write a EOS token to the stream
    strm->stream_write(EOS, 1);
  }
}


void stream_free(ScoreStream *strm) {


  // make sure this is not a double free.
  if (strm->consumerFreed) {
    cerr << "SCORESTREAMERR: TRYING TO DOUBLE FREE A STREAM! " << 
      (unsigned int) strm << endl;
    exit(1);
  }

  if (TIMEACC) {
    if (strm->readThreadCounter != NULL) {
      int tmp =  strm->get_stream_tokens_read() + 
	strm->get_stream_tokens_eos();
      strm->readThreadCounter->record->numOfUserStreamOPs += tmp;
      strm->readThreadCounter->record->numOfStreamReadOPs += 
	strm->get_stream_tokens_read();
      strm->readThreadCounter->record->numOfStreamEosOPs += 
	strm->get_stream_tokens_eos();
      strm->readThreadCounter->record->streamThreadTime +=
	strm->threadTimeRead[0];
    } else if (strm->writeThreadCounter == NULL) {
      // the reason for the else clause is that 
      // this is to get around an unknown bug
      // where the ptr gets reset to some unknown value
      ScoreThreadCounter *ptr = strm->getThreadCounterPtr();
      if (ptr != NULL) {
	ptr->record->numOfStreamReadOPs += strm->get_stream_tokens_read();
	ptr->record->numOfStreamEosOPs += strm->get_stream_tokens_eos();
      } else {
	cerr << "ERROR: stream_free - ptr is NULL" << endl;
	exit(-1);
      }
    }
  }

  if (!(strm->sched_isStitch)) {
    if (VERBOSEDEBUG || DEBUG || VERBOSE_STREAM) {
      cerr << "[SID=" << strm->streamID << "]   stream_free called" << endl;
    }
    
    if (USE_POLLING_STREAMS) {
      strm->acquire.sem_num = 0;
      PRINT_SEM(b_acquire,strm);
      while(semop(ScoreStream::doneSemId, &(strm->acquire), 1) == -1) {
	perror("semop -- stream_free -- acquire ");
	fprintf(stderr, "ScoreStream::doneSemId = %d\n", ScoreStream::doneSemId);
	strm->print(stderr);
	if (errno != EINTR)
	  exit(errno);
      }
      PRINT_SEM(acquire,strm);
    } else {
      // want to acquire the mutex DONE_MUTEX first
      
      strm->acquire.sem_num = DONE_MUTEX;
      
      while (semop(strm->semid, &(strm->acquire), 1) == -1){
	perror("semop -- stream_free -- acquire ");
	if (errno != EINTR)
	  exit(errno);
      }
    }
    
    if (VERBOSEDEBUG || DEBUG || VERBOSE_STREAM) {
      cerr << "[SID=" << strm->streamID << "]   stream_free mutex acquired" << 
	endl;
    }
    
    strm->consumerFreed = 1;
    
    if (strm->isGCable()) { // recycle everything
      if (VERBOSEDEBUG || DEBUG || VERBOSE_STREAM) {
	cerr << "[SID=" << strm->streamID << "]   stream_free GCing stream" <<
	  endl;
      }
      
      stream_gc(strm);

      if (USE_POLLING_STREAMS) {
	struct sembuf release;
	release.sem_num = 0;
	release.sem_op = 1;
	release.sem_flg = 0; // SEM_UNDO;
	PRINT_SEM(b_release,0);
	while(semop(ScoreStream::doneSemId, &release, 1) == -1) {
	  perror("semop -- stream_free -- release ");
	  if (errno != EINTR) 
	    exit(errno);
	}
      }
      PRINT_SEM(release,0);
    } else {
      char shouldDetach = 0;
      
      if (strm->snkFunc == STREAM_OPERATOR_TYPE) {
	if (strm->interProcess) {
	  shouldDetach = 1;
	}
      }
      
      if (VERBOSEDEBUG || DEBUG || VERBOSE_STREAM) {
	cerr << "[SID=" << strm->streamID << "]   stream_free is not GCing " <<
	  "stream" << endl;
      }
     
      if (USE_POLLING_STREAMS) {
	strm->release.sem_num = 0;
	PRINT_SEM(b_release,strm);
	while(semop(ScoreStream::doneSemId, &(strm->release), 1) == -1) {
	  perror("semop -- stream_free -- release ");
	  if (errno != EINTR) 
	    exit(errno);
	}
	PRINT_SEM(release,strm);
      } else { 
	// need to release the semaphore
	strm->release.sem_num = DONE_MUTEX;
	while (semop(strm->semid, &(strm->release), 1) == -1){
	  perror("semop -- stream_free -- release ");
	  if (errno != EINTR)
	    exit(errno);
	} 
      }
      
      // if the sink is an operator, then we will not get a
      // stream_free_hw later on, so detach the segment now.
      if (shouldDetach) {
        strm->memoized_runtimePtr = NULL;
	shmdt((char *)strm); // detach memory segment
      }
    }
  } else {
    if (VERBOSEDEBUG || DEBUG || VERBOSE_STREAM) {
      cerr << "[SID=" << strm->streamID << "]   stream_free called" << endl;
    }
    
    if (USE_POLLING_STREAMS) {
      strm->acquire.sem_num = 0;
      PRINT_SEM(b_acquire,strm);
      while(semop(ScoreStream::doneSemId, &(strm->acquire), 1) == -1) {
	perror("semop -- stream_close -- acquire");
	if (errno != EINTR)
	  exit(errno);
      }
      PRINT_SEM(acquire,strm);
    } else {
      while (sem_wait(&(strm->sem_DONE_MUTEX)) == -1){
	perror("sem_wait -- stream_free -- DONE_MUTEX ");
	if (errno != EINTR)
	  exit(errno);
      }
    }

    strm->consumerFreed = 1;
    
    if (strm->isGCable()) { // recycle everything
      // have to do this here since this is the only time we know if we
      // can truly GC this stream!
      ScoreStreamStitch *strmStitch = (ScoreStreamStitch *) strm;

      strmStitch->reset();
      SCORECUSTOMSTACK_PUSH(strm->sched_spareStreamStitchList, strm);
    } else {
      if (!USE_POLLING_STREAMS) {
	while (sem_post(&(strm->sem_DONE_MUTEX)) == -1){
	  perror("sem_post -- stream_free -- DONE_MUTEX ");
	  if (errno != EINTR)
	    exit(errno);
	}
      }
    }

    if (USE_POLLING_STREAMS) {
      strm->release.sem_num = 0;
      PRINT_SEM(b_release,strm);
      while (semop(ScoreStream::doneSemId, &(strm->release), 1) == -1) {
	perror("semop -- stream_close -- release ");
	if (errno != EINTR)
	  exit(errno);
      }
    }
    PRINT_SEM(release,strm);
  }
}


void stream_close_hw(ScoreStream *strm) {
  if (!(strm->sched_isStitch)) {
    if (VERBOSEDEBUG || DEBUG || VERBOSE_STREAM) {
      cerr << "[SID=" << 
	strm->streamID << "]   stream_close_hw called" << endl;
    }

    if (USE_POLLING_STREAMS) {
      strm->acquire.sem_num = 0;
      PRINT_SEM(b_acquire,strm);
      while(semop(ScoreStream::doneSemId, &(strm->acquire), 1) == -1) {
	perror("semop -- stream_close_hw -- acquire");
	if (errno != EINTR)
	  exit(errno);
      }
      PRINT_SEM(acquire,strm);
    } else {    
      // want to acquire the mutex DONE_MUTEX first
      
      strm->acquire.sem_num = DONE_MUTEX;
      
      while (semop(strm->semid, &(strm->acquire), 1) == -1){
	perror("semop -- stream_close_hw -- acquire ");
	if (errno != EINTR)
	  exit(errno);
      }
    }
    
    if (VERBOSEDEBUG || DEBUG || VERBOSE_STREAM) {
      cerr << "[SID=" << 
	strm->streamID << "]   stream_close_hw mutex acquired" << endl;
    }
    
    strm->producerClosed_hw = 1;
    
    if (strm->isGCable()) { // recycle everything
      if (VERBOSEDEBUG || DEBUG || VERBOSE_STREAM) {
	cerr << "[SID=" << strm->streamID << "]   stream_close_hw GCing " <<
	  "stream" << endl;
      }
      
      stream_gc(strm);

      if (USE_POLLING_STREAMS) {
	struct sembuf release;
	release.sem_num = 0;
	release.sem_op = 1;
	release.sem_flg = 0; // SEM_UNDO;
	PRINT_SEM(b_release,0);
	while(semop(ScoreStream::doneSemId, &release, 1) == -1) {
	  perror("semop -- stream_close_hw -- release ");
	  if (errno != EINTR)
	    exit(errno);
	}
      }
      PRINT_SEM(release,0);
    } else {
      char shouldDetach = 0;
      
      if (strm->interProcess) {
	shouldDetach = 1;
      }
      
      if (VERBOSEDEBUG || DEBUG || VERBOSE_STREAM) {
	cerr << "[SID=" << strm->streamID << "]   stream_close_hw is not " <<
	  "GCing stream" << endl;
      }
     
      if (USE_POLLING_STREAMS) {
	strm->release.sem_num = 0;
	PRINT_SEM(b_release,strm);
	while(semop(ScoreStream::doneSemId, &(strm->release), 1) == -1) {
	  perror("semop -- stream_close_hw -- release ");
	  if (errno != EINTR)
	    exit(errno);
	}
	PRINT_SEM(release,strm);
      } else { 
	// need to release the semaphore
	strm->release.sem_num = DONE_MUTEX;
	while (semop(strm->semid, &(strm->release), 1) == -1){
	  perror("semop -- stream_close_hw -- release ");
	  if (errno != EINTR)
	    exit(errno);
	}
      }

      if (shouldDetach) {
        strm->memoized_runtimePtr = NULL;
	shmdt((char *)strm); // detach memory segment
      }
    }
  } else {
    if (VERBOSEDEBUG || DEBUG || VERBOSE_STREAM) {
      cerr << "[SID=" << 
	strm->streamID << "]   stream_close_hw called" << endl;
    }

    if (USE_POLLING_STREAMS) {
      strm->acquire.sem_num = 0;
      PRINT_SEM(b_acquire,strm);
      while(semop(ScoreStream::doneSemId, &(strm->acquire), 1) == -1) {
	perror("semop -- stream_close -- acquire");
	if (errno != EINTR)
	  exit(errno);
      }
      PRINT_SEM(acquire,strm);
    } else {
      while (sem_wait(&(strm->sem_DONE_MUTEX)) == -1){
	perror("sem_wait -- stream_close_hw -- DONE_MUTEX ");
	if (errno != EINTR)
	  exit(errno);
      }
    }

    strm->producerClosed_hw = 1;
    
    if (strm->isGCable()) { // recycle everything
      // have to do this here since this is the only time we know if we
      // can truly GC this stream!
      ScoreStreamStitch *strmStitch = (ScoreStreamStitch *) strm;

      strmStitch->reset();
      SCORECUSTOMSTACK_PUSH(strm->sched_spareStreamStitchList, strm);
    } else {
      if (!USE_POLLING_STREAMS) {
	while (sem_post(&(strm->sem_DONE_MUTEX)) == -1){
	  perror("sem_post -- stream_close_hw -- DONE_MUTEX ");
	  if (errno != EINTR)
	    exit(errno);
	}
      }
    }

    if (USE_POLLING_STREAMS) {
      strm->release.sem_num = 0;
      PRINT_SEM(b_release,strm);
      while (semop(ScoreStream::doneSemId, &(strm->release), 1) == -1) {
	perror("semop -- stream_close -- release ");
	if (errno != EINTR)
	  exit(errno);
      }
      PRINT_SEM(release,strm);
    }

  }
}


void stream_free_hw(ScoreStream *strm) {


  if (!(strm->sched_isStitch)) {
    if (VERBOSEDEBUG || DEBUG) {
      cerr << "[SID=" << strm->streamID << "]   stream_free_hw called" << endl;
    }
    
    if (USE_POLLING_STREAMS) {
      strm->acquire.sem_num = 0;
      PRINT_SEM(b_acquire,strm);
      while (semop(ScoreStream::doneSemId, &(strm->acquire), 1) == -1){
	perror("semop -- stream_free_hw -- acquire ");
	if (errno != EINTR)
	  exit(errno);
      }
      PRINT_SEM(acquire,strm);
    } else {
      // want to acquire the mutex DONE_MUTEX first
      
      strm->acquire.sem_num = DONE_MUTEX;
      
  if(errno) {
    perror("stupid fuck");
  }
      while (semop(strm->semid, &(strm->acquire), 1) == -1){
	perror("semop -- stream_free_hw -- acquire ");
	if (errno != EINTR)
	  exit(errno);
      }
    }
    
    if (VERBOSEDEBUG || DEBUG || VERBOSE_STREAM) {
      cerr << "[SID=" << strm->streamID << "]   stream_free_hw mutex acquired" 
	   << endl;
    }
    
    strm->consumerFreed_hw = 1;
    
    if (strm->isGCable()) { // recycle everything
      if (VERBOSEDEBUG || DEBUG || VERBOSE_STREAM) {
	cerr << "[SID=" << strm->streamID << "]   stream_free_hw GCing " <<
	  "stream" << endl;
      }
      
      stream_gc(strm);

      if (USE_POLLING_STREAMS) {
	struct sembuf release;
	release.sem_num = 0;
	release.sem_op = 1;
	release.sem_flg = 0; // SEM_UNDO;
	PRINT_SEM(b_release,0);
	while (semop(ScoreStream::doneSemId, &release, 1) == -1){
	  perror("semop -- stream_free_hw -- release ");
	  if (errno != EINTR)
	    exit(errno);
	} 
	PRINT_SEM(release,0);
      }
    } else {
      char shouldDetach = 0;
      
      if (strm->interProcess) {
	shouldDetach = 1;
      }
      
      if (VERBOSEDEBUG || DEBUG || VERBOSE_STREAM) {
	cerr << "[SID=" << strm->streamID << "]   stream_free_hw is not " <<
	  "GCing stream" << endl;
      }

      if (USE_POLLING_STREAMS) {
	strm->release.sem_num = 0;
	PRINT_SEM(b_release,strm);
	while (semop(ScoreStream::doneSemId, &(strm->release), 1) == -1){
	  perror("semop -- stream_free_hw -- release ");
	  if (errno != EINTR)
	    exit(errno);
	} 
	PRINT_SEM(release,strm);
      } else {
	// need to release the semaphore
	strm->release.sem_num = DONE_MUTEX;
	while (semop(strm->semid, &(strm->release), 1) == -1){
	  perror("semop -- stream_free_hw -- release ");
	  if (errno != EINTR)
	    exit(errno);
	} 
      }
      
      if (shouldDetach) {
        strm->memoized_runtimePtr = NULL;
	shmdt(strm); // detach memory segment
      }
    }
  } else {
    if (VERBOSEDEBUG || DEBUG || VERBOSE_STREAM) {
      cerr << "[SID=" << strm->streamID << "]   stream_free_hw called" << endl;
    }
    
    if (USE_POLLING_STREAMS) {
      strm->acquire.sem_num = 0;
      PRINT_SEM(b_acquire,strm);
      while(semop(ScoreStream::doneSemId, &(strm->acquire), 1) == -1) {
	perror("semop -- stream_close -- acquire");
	if (errno != EINTR)
	  exit(errno);
      }
      PRINT_SEM(acquire,strm);
    } else {
      while (sem_wait(&(strm->sem_DONE_MUTEX)) == -1){
	perror("sem_wait -- stream_free_hw -- DONE_MUTEX ");
	if (errno != EINTR)
	  exit(errno);
      }
    }

    strm->consumerFreed_hw = 1;
    
    if (strm->isGCable()) { // recycle everything
      // have to do this here since this is the only time we know if we
      // can truly GC this stream!
      ScoreStreamStitch *strmStitch = (ScoreStreamStitch *) strm;

      strmStitch->reset();
      SCORECUSTOMSTACK_PUSH(strm->sched_spareStreamStitchList, strm);
    } else { 
      if (!USE_POLLING_STREAMS) {
	while (sem_post(&(strm->sem_DONE_MUTEX)) == -1){
	  perror("sem_post -- stream_free_hw -- DONE_MUTEX ");
	  if (errno != EINTR)
	    exit(errno);
	}
      }
    }

    if (USE_POLLING_STREAMS) {
      strm->release.sem_num = 0;
      PRINT_SEM(b_release,strm);
      while (semop(ScoreStream::doneSemId, &(strm->release), 1) == -1) {
	perror("semop -- stream_close -- release ");
	if (errno != EINTR)
	  exit(errno);
      }
      PRINT_SEM(release,strm);
    }
    
  }
}


void stream_gc(ScoreStream *strm) {
     
  int streamID = strm->streamID;
     
  if (VERBOSEDEBUG || DEBUG || VERBOSE_STREAM) {
    cerr << "[SID=" << streamID << "]  stream_gc entered" << endl;
  }

  assert(!strm->sched_isStitch);
     
  delete strm;  

  if (VERBOSEDEBUG || DEBUG || VERBOSE_STREAM) {
    cerr << "[SID=" << streamID << "]  stream_gc exited" << endl;
  }   
}


SCORE_STREAM_ID streamOBJ_to_ID(ScoreStream *strm) {
  strm->interProcess = 1; // set the flag
  return(strm->streamID);
}


ScoreStream *streamID_to_OBJ(SCORE_STREAM_ID id) {

  ScoreStream *shmptr;

  if (VERBOSEDEBUG || DEBUG) {
    cout << "   ID_to_OBJ " << endl;
  }

  if ((shmptr=(ScoreStream *)shmat(id, 0, 0))==(ScoreStream *) -1) {
    perror("shmptr -- streamID_to_OBJ -- attach ");
    exit(errno);
  }

  // make sure the runtime does not have 2 different pointers to the
  // same shared memory segment!
  // NOTE: IS THERE ANY REAL POTENTIAL FOR RACE CONDITIONS HERE? I DON'T
  //       BELIEVE SO SINCE THE MODIFICATION OF MEMOIZED_RUNTIMEPTR SHOULD
  //       BE SEQUENTIALIZED IN THE SCHEDULER... -mmchu 03/14/00
  if (shmptr->memoized_runtimePtr == NULL) {
    shmptr->memoized_runtimePtr = shmptr;
  } else {
    ScoreStream *memoizedPtr = shmptr->memoized_runtimePtr;

    shmdt((char *) shmptr);

    shmptr = memoizedPtr;

    shmptr->interProcess = 0; // unset the flag
  }

  if (VERBOSEDEBUG || DEBUG) {


    if (id != shmptr->streamID) {
      perror("shmptr -- something weird is going on");
      exit(errno);
    }
    printf("score stream ID passing in is: %d\n",(int)id);
    printf("score stream ID got back is: %d\n",(int)shmptr->streamID);
    cout << "attach address is: " << shmptr << endl;
  }

  return (ScoreStream *)shmptr;
}


void ScoreStream::syncSchedToReal() {
  src = sched_src;
  sink = sched_sink;
  isCrossCluster = sched_isCrossCluster;
  srcFunc = sched_srcFunc;
  snkFunc = sched_snkFunc;
  srcNum = sched_srcNum;
  snkNum = sched_snkNum;
  srcIsDone = sched_srcIsDone;
  sinkIsDone = sched_sinkIsDone;
}


// required by LEDA for use with lists/etc.

// provides comparison operation between ScoreStreams.
// NOTE: Right now, we only say 2 streams are equal if their pointers
//       are equal. Otherwise, less than/greater than is determined
//       simply by their pointer values.
int compare(ScoreStream * const & left, ScoreStream * const & right) {
  if (left == right) {
    return(0);
  } else if (left < right) {
    return(-1);
  } else {
    return(1);
  }
}


void ScoreStream::print(FILE *f)
{
  fprintf(f, "STREAM[streamID=%d,ptr=%u](tag_copy1=%x)\n", 
	  streamID, (unsigned int)this, tag_copy1);

  static const char *func_str[] = { "OPER", "PAGE", "SEG" };

  assert((srcFunc > 0) && (srcFunc < 4));

  fprintf(f, "\t src: %s.out%d [%u]", func_str[srcFunc - 1],
	  srcNum, (unsigned int)src);
  if (srcFunc > 1) { // it's PAGE or Segment
    assert(src);
    fprintf(f, "uniqTag = %d ", src->uniqTag);
    if (src->isPage()) {
      fprintf(f, "\'%s\'", ((ScorePage*)src)->source);
    }
  } 
  fprintf(f, "\n");

  assert((snkFunc > 0) && (snkFunc < 4));

  fprintf(f, "\tsink: %s.in%d [%u]", func_str[snkFunc - 1], 
	  snkNum, (unsigned int)sink);
  if (snkFunc > 1) { // it's PAGE or Segment
    assert(sink);
    fprintf(f, "uniqTag = %d ", sink->uniqTag);
    if (sink->isPage()) {
      fprintf(f, "\'%s\'", ((ScorePage*)sink)->source);
    }
  } 
  fprintf(f, "\n");
}


void ScoreStream::incrementInputConsumption()
{
  if (snkFunc != STREAM_OPERATOR_TYPE) {
    sink->incrementInputConsumption(snkNum);
  }
}
