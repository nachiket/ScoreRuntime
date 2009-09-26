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
// $Revision: 1.80 $
//
//////////////////////////////////////////////////////////////////////////////

#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include "ScoreSimulator.h"
#include "ScoreScheduler.h"
#include "ScoreVisualization.h"
#include "ScoreRuntime.h"
#include "ScoreConfig.h"
#include "ScorePlock.h"
#include <stdio.h>

///-----------choose scheduler type----------
#if defined(DYNAMIC_SCHEDULER)
#include "ScoreSchedulerDynamic.h"
#elif defined(STATIC_SCHEDULER)
#include "ScoreSchedulerStatic.h"
#elif defined(RANDOM_SCHEDULER)
#include "ScoreSchedulerRandom.h"
#else
#error "scheduler type was not chosen correctly"
#endif

///------------------------------------------


// this is an instantiation of the simulator (ScoreSimulator) class.
ScoreSimulator *simulator = NULL;

// this is an instantiation of the scheduler (ScoreScheduler) class.
ScoreScheduler *scheduler = NULL;

// this is an instantiation of the visualization manager.
ScoreVisualization *visualization = NULL;

// this is a pointer to the filename that will contain
// frames of simulator state
char *visualFile = NULL;
extern bool visualFile_complete;

bool outputNetlist = false;
bool uniqNetlistRes = false;
char *netlistDirName = 0;

#if GET_FEEDBACK
// this describes what should feedback module be doing
ScoreFeedbackMode gFeedbackMode = NOTHING;
int gFeedbackSampleFreq = 10000;
FILE *schedTemplateFile = 0;
char *gFeedbackFilePrefix = 0;
#endif

bool PROFILE_VERBOSE = true;

// this is the IPC channel id the IPC thread reads from.
static int ipcID = -1;

// used to communicate between the scheduler and simulator.
SCORE_STREAM toSimulator = NULL;
SCORE_STREAM fromSimulator = NULL;

// this is used to make sure only 1 thread is in the cleanup code at a time!
pthread_mutex_t cleanupMutex;
pthread_mutex_t cleanupHandlerMutex;


// flags.
char exitOnIdle;
char noDeadlockDetection;
char noImplicitDoneNodes;
char stitchBufferDontCare;


char isPseudoIdeal;

// the file which is used to account for reconfiguration commands on ts by ts
// basis. After the SCORE_EVENT_IDLE is sent to the sim, the file is closed
FILE *gReconfigAcctFile = 0;


// this stores the pointer to the old SIGINT interrupt handler.
static void (*oldSIGINTHandler)(int) = NULL;


// this is ONE done semaphore to be used as a critical section for 
// all stream done/free synchronizations (may be we do not need it)
int gDoneSemId = -1;

#if 0
#define PRINT_SIZE(__s__)  fprintf(stderr, "%s = %d\n", # __s__, sizeof(__s__))
#else 
#define PRINT_SIZE(__s__)
#endif

#if TRACK_LAST_VISITED_NODE
extern ScoreGraphNode *lastVisitedSimNode;
#endif

void  createNetlistDirs();

// this will initialize/register the scheduler as well as spawn the 2
// scheduler threads.
int main(int argc, char *argv[]) {

  PRINT_SIZE(ScoreStream);
  PRINT_SIZE(ScoreStreamStitch);
  PRINT_SIZE(ScoreSegment);
  PRINT_SIZE(ScoreGraphNode);
  PRINT_SIZE(ScorePage);
  PRINT_SIZE(ScoreSchedulerDynamic);
  PRINT_SIZE(ScoreSimulator);

  pthread_t ipcThread, mainSchedulerThread, mainSimulatorThread;
  key_t ipcKey;
  char *visualizationLogfileName = NULL;
  int i;

  if (USE_POLLING_STREAMS) { 
    assert(gDoneSemId == -1);
    assert(ScoreStream::doneSemId == -1);
    
    static union semun arg;
    static ushort start_val;
    start_val = 1;
    arg.array = &start_val;
    // create gDoneSemId semaphore
    if ((gDoneSemId = semget(0xfeedbabe, 1, IPC_EXCL | IPC_CREAT | 0666))
	!= -1) {
      if (semctl(gDoneSemId, 0, SETALL, arg) == -1) {
	perror("gDoneSemId -- semctl -- initialization");
	exit(errno);
      }
    }
    else {
      perror("gDoneSemId -- semget -- creation ");
      exit(errno);
    }

    ScoreStream::doneSemId = gDoneSemId;

    //cerr << "gDoneSemId = " << gDoneSemId << endl;
    
    
  }

  // register out cleanup signal handler to make sure stuff is cleaned up
  // properly before we exit (even if you control-C us!)!
  oldSIGINTHandler = signal(SIGINT, &cleanupSigHandler);

  // get the command-line arguments.
  // FIX ME! SHOULD DO ERROR CHECKING!
  // FIX ME! MAY WANT TO MAKE OTHER THINGS PARAMETERIZABLE LATER!
  i = 1;
  exitOnIdle = 0;
  noDeadlockDetection = 0;
  noImplicitDoneNodes = 0;
  stitchBufferDontCare = 0;
  isPseudoIdeal = 0;
  while (i < argc) {
    char *currentArg = argv[i];

    i++;

    if ((strcmp(currentArg, "-help") == 0) ||
        (strcmp(currentArg, "-h") == 0)) {
      cout << "USAGE: " << argv[0] << " [OPTIONS]" << endl;
      cout << "   -h\n" <<
	"      this help message!" << endl;
      cout << "   -pp X\n" <<
        "      # of physical compute pages (CP)" << endl;
      cout << "   -ps X\n" <<
        "      # of physical memory segments (CMB)" << endl;
      cout << "   -sz X\n" <<
	"      size of each physical memory segment" << endl;
      cout << "   -vl X\n" <<
	"      turn on \"tony-style\" visualization logs (specify filename)" << endl;
      cout << "   -eoi\n" <<
	"      exit on idle" << endl;
      cout << "   -ndld\n" <<
	"      do not run deadlock (bufferlock) detection" << endl;
      cout << "   -nidn\n" <<
        "      do not make nodes implicitly done" << endl;
      cout << "   -sbdc\n" <<
        "      this tells the scheduler not to consider stitch buffers in a cluster node list until there are only stitch buffers there" << endl;
      cout << "   -score_timeslice X\n" <<
	"      # of cycles per timeslice" << endl;
      cout << "   -score_fake_scheduler_time X\n" <<
        "      # of cycles to fake for the scheduler time" << endl;
      cout << "   -sim_cost_getarraystatus X\n" <<
	"      # of cycles to get array status" << endl;
      cout << "   -sim_cost_startpage X\n" <<
	"      # of cycles to start a page" << endl;
      cout << "   -sim_cost_stoppage X\n" <<
	"      # of cycles to stop a page" << endl;
      cout << "   -sim_cost_startsegment X\n" <<
	"      # of cycles to start a segment" << endl;
      cout << "   -sim_cost_stopsegment X\n" <<
	"      # of cycles to stop a segment" << endl;
      cout << "   -sim_cost_dumppagestate X\n" <<
	"      # of cycles to dump page state to CMB" << endl;
      cout << "   -sim_cost_dumppagefifo X\n" <<
	"      # of cycles to dump page FIFO to CMB" << endl;
      cout << "   -sim_cost_loadpageconfig X\n" <<
	"      # of cycles to load page config from CMB" << endl;
      cout << "   -sim_cost_loadpagestate X\n" <<
	"      # of cycles to load page state from CMB" << endl;
      cout << "   -sim_cost_loadpagefifo X\n" <<
	"      # of cycles to load page FIFO from CMB" << endl;
      cout << "   -sim_cost_getsegmentpointers X\n" <<
	"      # of cycles to get the segment pointers" << endl;
      cout << "   -sim_cost_dumpsegmentfifo X\n" <<
	"      # of cycles to dump segment FIFO to CMB" << endl;
      cout << "   -sim_cost_setsegmentconfigpointers X\n" <<
	"      # of cycles to set the segment pointers" << endl;
      cout << "   -sim_cost_changesegmentmode X\n" <<
	"      # of cycles to change the segment mode" << endl;
      cout << "   -sim_cost_changesegmenttraandpboandmax X\n" <<
	"      # of cycles to change the TRA/PBO/MAX of segment" << endl;
      cout << "   -sim_cost_resetsegmentdoneflag X\n" <<
	"      # of cycles to reset the segment done flag" << endl;
      cout << "   -sim_cost_loadsegmentfifo X\n" <<
	"      # of cycles to load segment FIFO from CMB" << endl;
      cout << "   -sim_cost_memxferprimarytocmb X\n" <<
	"      # of cycles to transfer one byte from main memory to CMB" << endl;
      cout << "   -sim_cost_memxfercmbtoprimary X\n" <<
	"      # of cycles to transfer one byte from CMB to main memory" << endl;
      cout << "   -sim_cost_memxfercmbtocmb X\n" <<
	"      # of cycles to transfer one byte from CMB to CMB" << endl;
      cout << "   -sim_cost_connectstream X\n" <<
	"      # of cycles to connect two nodes via a stream" << endl;
      cout << "   -vf X\n" <<
	"      turn on \"yury-style\" visualization graphs (specify filename)"
	   << endl;
      cout << "   -vf_c X\n" << 
	"      turn on \"yury-style\" COMPLETE visualization graphs (specify filename)"
	   << endl;
      cout << "   -netlist\n" <<
	"      make netlists reflecting current placement on each timeslice\n";
      cout << "   -stall_threshold X\n" <<
        "      # of cycles a page/segment must be stalled before it is considered freeable" << endl;
      cout << "   -clusterfreeable_ratio X\n" <<
        "      ratio of nodes in a cluster that must be freeable for the cluster to be freeable" << endl;
      cout << "   -datasegmentblock_loadsize X\n" <<
        "      # of bytes of consecutive data resident at one time for a segment (and stitch buffers)" << endl;
      cout << "   -pageconfig_size X\n" <<
        "      # of bytes to store the configuration bits for a page" << endl;
      cout << "   -pagestate_size X\n" <<
        "      # of bytes to store the state bits for a page" << endl;
      cout << "   -pagefifo_size X\n" <<
        "      # of bytes to store the contents of the input FIFOs for a page" << endl;
      cout << "   -memsegfifo_size X\n" <<
        "      # of bytes to store the contents of the input FIFOs for a segment" << endl;
      cout << "   -num_consecutive_deadlocks_to_kill X\n" <<
        "      # of consecutive timeslices deadlock has to be detected before the runtime exits" << endl;
      cout << "   -ispseudoideal\n" <<
        "      pseudo-ideal setting (sched cost 0; sim cost 0; timeslice 1)" << endl;
#if ASPLOS2000
      cout << "   -numpageincp X\n" <<
	"      # of page nodes in a CP" << endl;
#endif
      cout << "   -makefeedback\n" <<
	"      forces creation of the feedback files for the user applications" << endl;
      cout << "   -readfeedback\n" <<
	"      instructs runtime to read existing feedback files for user applications" << endl;
      cout << "   -random_seed\n" <<
	"      specify randomized seed to be used in random num generator for scheduling\n";
      cout << "   -make_template <filename>\n" << 
	"      instructs the runtime to make a scheduling template for a process\n";
      cout << "   -reconfig_stats <filename>\n" <<
	"      instructs the simulator to emit reconfiguration statistics for each timeslice\n";
      cout << "   -sample_rates\n" <<
	"      sample rates a specified intervals\n";
      cout << "   -sample_freq <num>\n" <<
        "      specify sampling rate frequency\n";
      cout << "   -feedback_file_prefix <prefix>\n" <<
	"      specify the prefix given to feedback file\n";
      exit(0);
    } else if ((strcmp(currentArg, "-numphysicalpages") == 0) ||
	(strcmp(currentArg, "-pp") == 0)) {
      char *currentParameter = argv[i];

      i++;

      numPhysicalPages = atoi(currentParameter);
    } else if ((strcmp(currentArg, "-numphysicalsegments") == 0) ||
	       (strcmp(currentArg, "-ps") == 0)) {
      char *currentParameter = argv[i];

      i++;

      numPhysicalSegments = atoi(currentParameter);
    } else if ((strcmp(currentArg, "-physicalsegmentsize") == 0) ||
	       (strcmp(currentArg, "-sz") == 0)) {
      char *currentParameter = argv[i];

      i++;

      physicalSegmentSize = atoi(currentParameter);
    } else if ((strcmp(currentArg, "-visualizationlog") == 0) ||
	       (strcmp(currentArg, "-vl") == 0)) {
      char *currentParameter = argv[i];

      i++;
      if (TONY_GRAPH) {
	visualizationLogfileName = currentParameter;
      } else {
	cout << "Compiler flag TONY_GRAPH not turned on" << endl;
	cout << "-vl option will be ignored" << endl;
      }
    } else if ((strcmp(currentArg, "-exitonidle") == 0) ||
               (strcmp(currentArg, "-eoi") == 0)) {
      exitOnIdle = 1;
    } else if ((strcmp(currentArg, "-nodeadlockdetection") == 0) ||
               (strcmp(currentArg, "-ndld") == 0)) {
      noDeadlockDetection = 1;
    } else if ((strcmp(currentArg, "-noimplicitlydonenodes") == 0) ||
               (strcmp(currentArg, "-nidn") == 0)) {
      noImplicitDoneNodes = 1;
    } else if ((strcmp(currentArg, "-ispseudoideal") == 0) ||
               (strcmp(currentArg, "-ipi") == 0)) {
      isPseudoIdeal = 1;
      // FIX ME!
      SCORE_TIMESLICE = 100000;
      SCORE_STALL_THRESHOLD = 50000;
      SCORE_FAKE_SCHEDULER_TIME = 0;
      SIM_COST_GETARRAYSTATUS = 0;
      SIM_COST_STARTPAGE = 0;
      SIM_COST_STOPPAGE = 0;
      SIM_COST_STARTSEGMENT = 0;
      SIM_COST_STOPSEGMENT = 0;
      SIM_COST_DUMPPAGESTATE = 0;
      SIM_COST_DUMPPAGEFIFO = 0;
      SIM_COST_LOADPAGECONFIG = 0;
      SIM_COST_LOADPAGESTATE = 0;
      SIM_COST_LOADPAGEFIFO = 0;
      SIM_COST_GETSEGMENTPOINTERS = 0;
      SIM_COST_DUMPSEGMENTFIFO = 0;
      SIM_COST_SETSEGMENTCONFIGPOINTERS = 0;
      SIM_COST_CHANGESEGMENTMODE = 0;
      SIM_COST_CHANGESEGMENTTRAANDPBOANDMAX = 0;
      SIM_COST_RESETSEGMENTDONEFLAG = 0;
      SIM_COST_LOADSEGMENTFIFO = 0;
      SIM_COST_MEMXFERPRIMARYTOCMB = 0;
      SIM_COST_MEMXFERCMBTOPRIMARY = 0;
      SIM_COST_MEMXFERCMBTOCMB = 0;
      SIM_COST_CONNECTSTREAM = 0;
    } else if ((strcmp(currentArg, "-stitchbufferdontcare") == 0) ||
               (strcmp(currentArg, "-sbdc") == 0)) {
      stitchBufferDontCare = 1;
    } else if (strcmp(currentArg, "-score_timeslice") == 0) {
      char *currentParameter = argv[i];

      i++;

      SCORE_TIMESLICE = atoi(currentParameter);
    } else if (strcmp(currentArg, "-score_fake_scheduler_time") == 0) {
      char *currentParameter = argv[i];

      i++;

      SCORE_FAKE_SCHEDULER_TIME = atoi(currentParameter);
    } else if (strcmp(currentArg, "-sim_cost_getarraystatus") == 0) {
      char *currentParameter = argv[i];

      i++;

      SIM_COST_GETARRAYSTATUS = atoi(currentParameter);
    } else if (strcmp(currentArg, "-sim_cost_startpage") == 0) {
      char *currentParameter = argv[i];

      i++;

      SIM_COST_STARTPAGE = atoi(currentParameter);
    } else if (strcmp(currentArg, "-sim_cost_stoppage") == 0) {
      char *currentParameter = argv[i];

      i++;

      SIM_COST_STOPPAGE = atoi(currentParameter);
    } else if (strcmp(currentArg, "-sim_cost_startsegment") == 0) {
      char *currentParameter = argv[i];

      i++;

      SIM_COST_STARTSEGMENT = atoi(currentParameter);
    } else if (strcmp(currentArg, "-sim_cost_stopsegment") == 0) {
      char *currentParameter = argv[i];

      i++;

      SIM_COST_STOPSEGMENT = atoi(currentParameter);
    } else if (strcmp(currentArg, "-sim_cost_dumppagestate") == 0) {
      char *currentParameter = argv[i];

      i++;

      SIM_COST_DUMPPAGESTATE = atoi(currentParameter);
    } else if (strcmp(currentArg, "-sim_cost_dumppagefifo") == 0) {
      char *currentParameter = argv[i];

      i++;

      SIM_COST_DUMPPAGEFIFO = atoi(currentParameter);
    } else if (strcmp(currentArg, "-sim_cost_loadpageconfig") == 0) {
      char *currentParameter = argv[i];

      i++;

      SIM_COST_LOADPAGECONFIG = atoi(currentParameter);
    } else if (strcmp(currentArg, "-sim_cost_loadpagestate") == 0) {
      char *currentParameter = argv[i];

      i++;

      SIM_COST_LOADPAGESTATE = atoi(currentParameter);
    } else if (strcmp(currentArg, "-sim_cost_loadpagefifo") == 0) {
      char *currentParameter = argv[i];

      i++;

      SIM_COST_LOADPAGEFIFO = atoi(currentParameter);
    } else if (strcmp(currentArg, "-sim_cost_getsegmentpointers") == 0) {
      char *currentParameter = argv[i];

      i++;

      SIM_COST_GETSEGMENTPOINTERS = atoi(currentParameter);
    } else if (strcmp(currentArg, "-sim_cost_dumpsegmentfifo") == 0) {
      char *currentParameter = argv[i];

      i++;

      SIM_COST_DUMPSEGMENTFIFO = atoi(currentParameter);
    } else if (strcmp(currentArg, "-sim_cost_setsegmentconfigpointers") == 0) {
      char *currentParameter = argv[i];

      i++;

      SIM_COST_SETSEGMENTCONFIGPOINTERS = atoi(currentParameter);
    } else if (strcmp(currentArg, "-sim_cost_changesegmentmode") == 0) {
      char *currentParameter = argv[i];

      i++;

      SIM_COST_CHANGESEGMENTMODE = atoi(currentParameter);
    } else if (strcmp(currentArg, "-sim_cost_changesegmenttraandpboandmax") == 0) {
      char *currentParameter = argv[i];

      i++;

      SIM_COST_CHANGESEGMENTTRAANDPBOANDMAX = atoi(currentParameter);
    } else if (strcmp(currentArg, "-sim_cost_resetsegmentdoneflag") == 0) {
      char *currentParameter = argv[i];

      i++;

      SIM_COST_RESETSEGMENTDONEFLAG = atoi(currentParameter);
    } else if (strcmp(currentArg, "-sim_cost_loadsegmentfifo") == 0) {
      char *currentParameter = argv[i];

      i++;

      SIM_COST_LOADSEGMENTFIFO = atoi(currentParameter);
    } else if (strcmp(currentArg, "-sim_cost_memxferprimarytocmb") == 0) {
      char *currentParameter = argv[i];

      i++;

      SIM_COST_MEMXFERPRIMARYTOCMB = atof(currentParameter);
    } else if (strcmp(currentArg, "-sim_cost_memxfercmbtoprimary") == 0) {
      char *currentParameter = argv[i];

      i++;

      SIM_COST_MEMXFERCMBTOPRIMARY = atof(currentParameter);
    } else if (strcmp(currentArg, "-sim_cost_memxfercmbtocmb") == 0) {
      char *currentParameter = argv[i];

      i++;

      SIM_COST_MEMXFERCMBTOCMB = atof(currentParameter);
    } else if (strcmp(currentArg, "-sim_cost_connectstream") == 0) {
      char *currentParameter = argv[i];

      i++;

      SIM_COST_CONNECTSTREAM = atoi(currentParameter);
    } else if ((strcmp(currentArg, "-visualfile") == 0) ||
               (strcmp(currentArg, "-vf") == 0) || 
	       (strcmp(currentArg, "-vf_c") == 0)) {
      char *currentParameter = argv[i];
      
      i ++;

      if (!VISUALIZE_STATE) {
	cerr << "ScoreRuntime was NOT compiled with VISUALIZE_STATE=1" << endl;
	exit (1);
      }
      
      FILE *fp_tmp = NULL;
      
      if ((fp_tmp = fopen (currentParameter, "wb")) == NULL) {
        cerr << "WARNING: unable to open " << currentParameter << 
	  ". The simulation will proceed without visualization." << endl;
      } 
      else { 
        fclose (fp_tmp);
        visualFile = new char [strlen(currentParameter) + 1];

        memset (visualFile, '\0', strlen(currentParameter) + 1);
	
        strcpy (visualFile, currentParameter);
      }

      visualFile_complete = (strcmp(currentArg, "-vf_c") == 0);
    } if (!strcmp(currentArg, "-netlist")) {

      if (!VISUALIZE_STATE) {
	cerr << "ScoreRuntime was NOT compiled with VISUALIZE_STATE=1" << endl;
	exit (1);
      }

      outputNetlist = true;

    } if (!strcmp(currentArg, "-time_netlist")) {

      if (!VISUALIZE_STATE) {
	cerr << "ScoreRuntime was NOT compiled with VISUALIZE_STATE=1" << endl;
	exit (1);
      }

      outputNetlist = true;
      uniqNetlistRes = true;

    } else if (strcmp(currentArg, "-stall_threshold") == 0) {
      char *currentParameter = argv[i];

      i++;

      SCORE_STALL_THRESHOLD = atoi(currentParameter);
    } else if (strcmp(currentArg, "-clusterfreeable_ratio") == 0) {
      char *currentParameter = argv[i];

      i++;

      SCORE_CLUSTERFREEABLE_RATIO = atof(currentParameter);
    } else if (strcmp(currentArg, "-datasegmentblock_loadsize") == 0) {
      char *currentParameter = argv[i];

      i++;

      SCORE_DATASEGMENTBLOCK_LOADSIZE = atoi(currentParameter);
    } else if (strcmp(currentArg, "-pageconfig_size") == 0) {
      char *currentParameter = argv[i];

      i++;

      SCORE_PAGECONFIG_SIZE = atoi(currentParameter);
    } else if (strcmp(currentArg, "-pagestate_size") == 0) {
      char *currentParameter = argv[i];

      i++;

      SCORE_PAGESTATE_SIZE = atoi(currentParameter);
    } else if (strcmp(currentArg, "-pagefifo_size") == 0) {
      char *currentParameter = argv[i];

      i++;

      SCORE_PAGEFIFO_SIZE = atoi(currentParameter);
    } else if (strcmp(currentArg, "-memsegfifo_size") == 0) {
      char *currentParameter = argv[i];

      i++;

      SCORE_MEMSEGFIFO_SIZE = atoi(currentParameter);
    } else if (strcmp(currentArg, "-num_consecutive_deadlocks_to_kill") == 0) {
      char *currentParameter = argv[i];

      i++;

      SCORE_NUM_CONSECUTIVE_DEADLOCKS_TO_KILL = atoi(currentParameter);
#if ASPLOS2000
    } else if (strcmp(currentArg, "-numpageincp") == 0) {
      char *currentParameter = argv[i];

      i++;

      SCORE_NUM_PAGENODES_IN_CP = atoi(currentParameter);
#endif
#if GET_FEEDBACK
    } else if (strcmp(currentArg, "-makefeedback") == 0) {
      gFeedbackMode = MAKEFEEDBACK;
    } else if (strcmp(currentArg, "-readfeedback") == 0) {
      gFeedbackMode = READFEEDBACK;
    } else if (strcmp(currentArg, "-sample_rates") == 0) {
      gFeedbackMode = SAMPLERATES;
    } else if (strcmp(currentArg, "-sample_freq") == 0) {
      char *fr = argv[i++];
      sscanf(fr, "%d", &gFeedbackSampleFreq);
      assert(gFeedbackSampleFreq > 0);
    } else if (strcmp(currentArg, "-make_template") == 0) {
      char *filename = argv[i++];
      if (!(schedTemplateFile = fopen(filename, "w"))) {
	cerr << "RUNTIME ERROR: unable to open " << filename << " for write\n";
	exit(1);
      }
      gFeedbackMode = MAKEFEEDBACK;
    } else if (strcmp(currentArg, "-feedback_file_prefix") == 0) {
      gFeedbackFilePrefix = new char [strlen(argv[i]) + 1];
      strcpy(gFeedbackFilePrefix, argv[i++]);
#endif
#ifdef RANDOM_SCHEDULER
    } else if (strcmp(currentArg, "-r") == 0) {
      cerr << "DO NOT USE -r OPTION\nIt is only for debugging\n";
      exit(1);
      char *currentParameter = argv[i];
      i++;
      
      int count = atoi(currentParameter);
      for ( ; count > 0; count --)
	random();
    } else if (strcmp(currentArg, "-random_seed") == 0 ||
	       strcmp(currentArg, "-rs") == 0) {
      char *currentParameter = argv[i];
      i ++;
      char *end;

      unsigned int seed = (unsigned int) strtoul(currentParameter, &end, 10);
      srandom(seed);
#endif
    } else if (strcmp(currentArg, "-quiet_prof") == 0) {
      PROFILE_VERBOSE = false;

    } else if (strcmp(currentArg, "-reconfig_stats") == 0) {
      char *currentParameter = argv[i];
      i ++;
#if RECONFIG_ACCT
      if ((gReconfigAcctFile = fopen(currentParameter, "w")) == NULL) {
	cerr << "ERROR: unable to open " << currentParameter << endl;
	exit(1);
      }
#else 
      cerr << "ERROR: the ScoreRuntime was not compiled with RECONFIG_ACCT = 1\n";
      exit(1);
#endif
    }
  }

  // create appropriate directory structure for netlists
  createNetlistDirs();


#if ASPLOS2000
  numPhysicalPages = numPhysicalPages * SCORE_NUM_PAGENODES_IN_CP;
#endif

  SCORE_SEGMENTTABLE_LEVEL0SIZE =
    SCORE_PAGE_ALIGN(SCORE_DATASEGMENTBLOCK_LOADSIZE +
                     SCORE_MEMSEGFIFO_SIZE);
  SCORE_SEGMENTTABLE_LEVEL1SIZE =
    SCORE_PAGE_ALIGN(SCORE_PAGECONFIG_SIZE +
                     SCORE_PAGESTATE_SIZE +
                     SCORE_PAGEFIFO_SIZE);

  // register the cleanup routine to run at exit.
  if (pthread_mutex_init(&cleanupMutex, NULL) != 0) {
    cerr << "SCORERUNTIMEERR: Could not initialize the cleanup mutex!" << endl;
    exit(1);
  }
  if (pthread_mutex_init(&cleanupHandlerMutex, NULL) != 0) {
    cerr << "SCORERUNTIMEERR: Could not initialize the cleanup handler " <<
      "mutex!" << endl;
    exit(1);
  }
  if (atexit(&cleanup) != 0) {
    cerr << "Could not register the cleanup routine!" << endl;
    exit(1);
  }

  // create a visualization manager and start a visualization log if requested.
  if (visualizationLogfileName != NULL) {
    visualization = 
      new ScoreVisualization(numPhysicalPages, numPhysicalSegments);
    visualization->startVisualization(visualizationLogfileName);
  }

  // get and register the IPC channel id.
  ipcKey = ftok(".", 0);
  if (ipcKey == -1) {
    cerr << "Could not get IPC message key!" << endl;
    exit(1);
  }
  ipcID = msgget(ipcKey, IPC_CREAT|0666);
  if (set_schedulerid(ipcID) == -1) {
    exit(1);
  }

/* 
  // Nachiket removed this to avoid perfctr bs
  ScoreThreadCounter *shmptr = new ScoreThreadCounter(RUNTIME);

  // instantiate the streams used to talk between the scheduler and simulator.
  toSimulator = NEW_SCORE_STREAM_ARRAY();
  if (toSimulator == NULL) {
    cerr << "Insufficient memory to create toSimulator stream!" << endl;
    exit(1);
  }
  SCORE_MARKSTREAM(toSimulator,shmptr); 

  fromSimulator = NEW_SCORE_STREAM_ARRAY();
  if (fromSimulator == NULL) {
    cerr << "Insufficient memory to create fromSimulator stream!" << endl;
    exit(1);
  }
  SCORE_MARKSTREAM(fromSimulator,shmptr); 
*/

  if (VERBOSEDEBUG || DEBUG) {
    cout << "=========================================" << endl;
    
    cout << "ipcID: " << ipcID << endl;
    
    cout << "numPhysicalPages: " << numPhysicalPages << endl;
    cout << "numPhysicalSegments: " << numPhysicalSegments << endl;
    cout << "physicalSegmentSize: " << physicalSegmentSize << endl;
    
    cout << "=========================================" << endl;
  }

  // instantiate the IPC, main scheduler, and main simulator threads.
  if (VERBOSEDEBUG || DEBUG) {
    cout << "Forking off IPC thread..." << endl;
  }
  if (pthread_create(&ipcThread, NULL, ipc_thread_run, NULL) != 0) {
    cerr << "Could not create IPC thread!" << endl;
    exit(1);
  }

  if (VERBOSEDEBUG || DEBUG) {
    cout << "Forking off main scheduler thread..." << endl;
  }

  pthread_attr_t schThreadAttribute;
  pthread_attr_init(&schThreadAttribute);

  if (pthread_create(&mainSchedulerThread, &schThreadAttribute, 
		     mainScheduler_thread_run, NULL) != 0) {
    cerr << "Could not create main scheduler thread!" << endl;
    exit(1);
  }

  if (VERBOSEDEBUG || DEBUG) {
    cout << "Forking off main simulator thread..." << endl;
  }

  // creating simulator is different because we want to 
  // tell the OS scheduler to keep track of context switch time
  // for this thread

  pthread_attr_t simThreadAttribute;
  pthread_attr_init(&simThreadAttribute);

  if (pthread_create(&mainSimulatorThread, &simThreadAttribute, 
		     mainSimulator_thread_run, NULL) != 0) {
    cerr << "Could not create main simulator thread!" << endl;
    exit(1);
  }

  // join with the outstanding threads.
  if (pthread_join(ipcThread, NULL) != 0) {
    cerr << "Could not join with IPC thread!" << endl;
    exit(1);
  }
  if (pthread_join(mainSchedulerThread, NULL) != 0) {
    cerr << "Could not join with main scheduler thread!" << endl;
    exit(1);
  }
  if (pthread_join(mainSimulatorThread, NULL) != 0) {
    cerr << "Could not join with main simulator thread!" << endl;
    exit(1);
  }

  return(0);
}


// this is the routine run by the IPC thread.
void *ipc_thread_run(void *data) {
  rmsgbuf *msgp = NULL;


  // allocate the message buffer.
  msgp = new struct rmsgbuf();
  if (msgp == NULL) {
    cerr << "Insufficient memory to instantiate IPC buffer!" << endl;
    exit(1);
  }

  // go into a loop to get messages from users.
  while (1) {
    int msgsz = -1;

    // receive the next message.
    msgsz = msgrcv(ipcID, msgp, sizeof(rmsgbuf),
                   SCORE_INSTANTIATE_MESSAGE_TYPE, 0);

    // process the message.
    if (msgsz == -1) {
      // if EINTR was received, let the error go silently. it is probably
      // being run under GDB.
      if (errno != EINTR) {
	cerr << "Error receiving IPC message! (errno = " << errno << ")" << 
	  endl;
	exit(-1);
      }
    } else {
      int slen;
      int alen;
      char *sname;
      char *argbuf;
      msqid_ds queueStat;
          
      // get the queue status.
      msgctl(ipcID, IPC_STAT, &queueStat);

      // get the components from the message.
      memcpy(&alen, msgp->mtext, 4);
      memcpy(&slen, msgp->mtext+4, 4);
      sname = new char[slen+1];
      memcpy(sname, msgp->mtext+8, slen);
      *(sname+slen) = '\0';
      argbuf = new char[alen];
      memcpy(argbuf, msgp->mtext+8+slen, alen);
      
      // send the shared object and argument information to the scheduler
      // to deal with.
      if (scheduler->addOperator(sname, argbuf, queueStat.msg_lspid) != 0) {
	cerr << "Error adding operator to scheduler! " <<
	     "(sname=" << sname << ", argbuf=" << argbuf << ")" << endl;
      }

      // cleanup.
      delete sname;
      sname = NULL;
      delete argbuf;
      argbuf = NULL;
    }
  }

  // deallocate the message buffer.
  delete msgp;
  msgp = NULL;

  pthread_exit(0);
  return(NULL);
}


// this is the routine run by the main scheduler thread.
void *mainScheduler_thread_run(void *data) {

  set_proc(PLOCK_SCHEDULER_CPU, "mainScheduler_thread_run");

  // instantiate the scheduler class.
  if (VERBOSEDEBUG || DEBUG) {
    cout << "Instantiating ScoreScheduler..." << endl;
  }
  ///-----------choose scheduler type----------
#if defined(DYNAMIC_SCHEDULER)
  scheduler = new ScoreSchedulerDynamic(exitOnIdle, noDeadlockDetection,
                                 noImplicitDoneNodes, stitchBufferDontCare);
#elif defined(STATIC_SCHEDULER)
  cerr << "******************STATIC INIT*************\n";
  scheduler = new ScoreSchedulerStatic(exitOnIdle, noDeadlockDetection,
                                 noImplicitDoneNodes, stitchBufferDontCare);
#elif defined(RANDOM_SCHEDULER)
  scheduler = new ScoreSchedulerRandom(exitOnIdle, noDeadlockDetection,
                                 noImplicitDoneNodes, stitchBufferDontCare);
#else
#error "scheduler type was not chosen correctly"
#endif

///------------------------------------------
  if (scheduler == NULL) {
    cerr << "Insufficient memory to instantiate the scheduler!" << endl;
    exit(1);
  }

  while (1) {
    if (waitForNextTimeslice() != 0) {
      exit(1);
    }

    // call schedule to do scheduling.
    scheduler->doSchedule();
  }

  pthread_exit(0);
  return(NULL);
}


// this is the routine run by the main simulator thread.
void *mainSimulator_thread_run(void *data) {

  set_proc(PLOCK_SIMULATOR_CPU, "mainSimulator_thread_run");

  // instantiate the simulator class.
  if (VERBOSEDEBUG || DEBUG) {
    cout << "Instantiating ScoreSimulator..." << endl;
  }
  simulator = new ScoreSimulator(toSimulator, fromSimulator);
  if (simulator == NULL) {
    cerr << "Insufficient memory to instantiate the simulator!" << endl;
    exit(1);
  }

  // run the simulator.
  simulator->run();

  pthread_exit(0);
  return(NULL);
}


// this will set the scheduler IPC id with /proc/schedulerid.
// Return: -1 upon error, 0 if successful.
int set_schedulerid(int id) {
  int file = -1;


  file = open("/proc/schedulerid", O_WRONLY);

  if (file < 0) {
    cerr << "Could not open /proc/schedulerid! (errno = " << 
             errno << ")" << endl;
    return(-1);
  }

  if (write(file, &id, 4) != 4) {
    cerr << "Could not write scheduler IPC id! (errno = " <<
            errno << ")" << endl;
    return(-1);
  }

  close(file);
  return(0);
}


// this performs the necessary cleanup on exiting.
void cleanup() {
  pthread_mutex_lock(&cleanupMutex);

  // unregister the scheduler.
  set_schedulerid(-1);

// need to somehow protect the this access since the simulator might be
// in the middle of accessing the streams.
#if 0
  // if the scheduler<->simulator streams were allocated, deallocate them.
  if (toSimulator != NULL) {
    // FIX ME! DO I HAVE TO CLOSE/FREE BEFORE DELETING?
    STREAM_CLOSE(toSimulator);
    STREAM_FREE(toSimulator);
    toSimulator = NULL;
  }
  if (fromSimulator != NULL) {
    // FIX ME! DO I HAVE TO CLOSE/FREE BEFORE DELETING?
    STREAM_CLOSE(fromSimulator);
    STREAM_FREE(fromSimulator);
    fromSimulator = NULL;
  }
#endif

  // if there was a visualization, close it.
  if (visualization != NULL) {
    visualization->stopVisualization_noclose();
    delete(visualization);
    visualization = NULL;
  }

  pthread_mutex_unlock(&cleanupMutex);
}


// this is a cleanup signal handler. it will attempt to cleanup stuff before
// the program is killed.
void cleanupSigHandler(int signum) {
  //cerr << "Clean up sig handler-- begin -- need acquire\n";

  pthread_mutex_lock(&cleanupHandlerMutex);

  //cerr << "Clean up sig handler-- begin\n";

#if TRACK_LAST_VISITED_NODE
  fprintf(stderr, "*** lastVisitedSimNode:\n");
  if (lastVisitedSimNode) {
    lastVisitedSimNode->print(stderr);
  } else {
    fprintf(stderr, "NULL\n");
  }
#endif

  // force a cleanup call.
  cleanup();

  //cerr << "Clean up sig handler-- end\n";

  pthread_mutex_unlock(&cleanupHandlerMutex);

  //cerr << "Clean up sig handler-- end -- all done\n";

  // force a "hard" exit.
  _exit(1);
}



////////////////////////////////////////////////////////////////////////
// createNetlistDirs();
//   all netlists will be placed into netlist/netlist_cps_cmbs.time/
//   this routine creates the directory structure.
////////////////////////////////////////////////////////////////////////
void createNetlistDirs()
{
  if (!outputNetlist) {
    assert(!netlistDirName);
    return;
  }

  // check whether netlist directory exists
  struct stat info;
        
  errno = 0;

  mode_t dirPerm =
    S_IREAD | S_IWRITE | S_IEXEC | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
  
  if (stat("netlist", &info)) { // check the existance of netlist directory
    if (errno == ENOENT) { // directory netlist does not exist
      if (mkdir("netlist", dirPerm)) {
	perror("error creating netlist directory:");
	exit(1);
      }
    } else {
      perror("error with stat(\"netlist\",...)");
      exit(1);
    }
  }

  if (!S_ISDIR(info.st_mode) && !errno) {
    fprintf(stderr, "error: netlist is NOT a directory\n");
    exit(1);
  }

  time_t now = time(0);
  char targetDirName[100];

  sprintf(targetDirName, "netlist/netlist_cp%d_cmb%d.%ld", 
	  numPhysicalPages, numPhysicalSegments, now);

  errno = 0;
  if (stat(targetDirName, &info)) {
    if (errno == ENOENT) { // need to create directory
      if (mkdir(targetDirName, dirPerm)) {
	perror("error creating netlist/netlist_... directory");
	exit(1);
      }
    } else {
      perror("error stat netlist/netlist_... directory");
    }
  }

  if (!S_ISDIR(info.st_mode) && !errno) {
    fprintf(stderr, "error: %s is NOT a directory\n", targetDirName);
    exit(1);
  }

  netlistDirName = new char[strlen(targetDirName) + 1];

  strcpy(netlistDirName, targetDirName);
}

