#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <assert.h>
#include <error.h>
#include <iostream>
#include <stdio.h>

#include "ScoreStream.h"

extern "C"
void _init()
{
  if (USE_POLLING_STREAMS) { 
    assert(ScoreStream::doneSemId == -1);
    
    static union semun arg;
    static ushort start_val;
    start_val = 1;
    arg.array = &start_val;
    // create gDoneSemId semaphore
    if ((ScoreStream::doneSemId = 
	 semget(0xfeedbabe, 1, IPC_CREAT | 0666)) == -1) {
      perror("_init ScoreStream::doneSemId -- semget -- creation ");
      exit(errno);
    }
    
#if 0
    cerr << "_init ScoreStream::doneSemId = " <<
      ScoreStream::doneSemId << endl;
#endif
  }

  // we must explicitely instantiate this list, since it never gets
  // constructed inside of so.
  //fprintf(stderr, "so helper: about to new\n");
  streamsToExpand = new list<ScoreStream*>;
  //fprintf(stderr, "so helper: done new -- %d\n", streamsToExpand->_magic);
  //fprintf(stderr, "add of streamsToExpand is %p\n", &streamsToExpand);
  //fprintf(stderr, "val of streamsToExpand is %p\n", streamsToExpand);
}
    
