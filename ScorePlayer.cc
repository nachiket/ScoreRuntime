/////////////////////////////////////////////////////////////////////////////
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
// SCORE visualization player
//
//////////////////////////////////////////////////////////////////////////////

#include <iostream>
#include <memory.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <ctype.h>

#include "ScorePlayer.h"
#include "ScoreStateGraph.h"


const char *DUMMY_NAME  = "dummy.vcg";
bool gSplitCPU = true;
bool gNOCPU = false;

void runPlayer(FILE* fp, pid_t child_pid); 
void makeDummyFile();
void show_env();
bool parseArg(char *str, int *number);
void PrintUsageAndDie();

int main (int argc, char *argv[])
{
  char *filename = 0;
  
  for (int i = 1; i < argc; i ++) {
    if (argv[i][0] == '-') { // deal with options
      if (!strcmp(argv[i], "-s") ||
	  !strcmp(argv[i], "-splitCPU")) {
	gSplitCPU = true;
      }
      else if (!strcmp(argv[i], "-ns") ||
	       !strcmp(argv[i], "-nosplitCPU")) {
	gSplitCPU = false;
      }
      else if (!strcmp(argv[i], "-no_cpu")) {
	gNOCPU = true;
      }
      else {
	PrintUsageAndDie();
      }

    }
    else { // assume that this is a filename
      if (filename) 
	PrintUsageAndDie();
      filename = argv[i];
    }
  }

  if (!filename) 
    PrintUsageAndDie();

  ScorePlayer player;
  
  if (player.Init(filename))
    player.Run();
  else {
    exit(1);
  }

  //unlink ("./vcg.errors");
  return 0;
}

void PrintUsageAndDie()
{
  cerr << "ScorePlayer: use ScorePlayer [-s/-splitCPU] "
       << "[-ns/-nosplitCPU] <log.file.name>" << endl;
  exit(1);
}


// Implementation of ScorePlayer

// release the memory occupied by the PlayerIndexItem's
ScorePlayer::~ScorePlayer()
{
  for (unsigned int i = 0; i < index.size(); i ++) 
    delete index[i];

  if (dataFile)
    fclose(dataFile);
}

// Initialize the player:
// (1) open the file req'd
// (2) build index
// (3) fork off a separate process to run xvcg
// return true, if everything is successful, otherwise false
bool ScorePlayer::Init(char *filename)
{
  if ((dataFile = fopen(filename, "r")) == NULL) {
    cerr << "ScorePlayer: ERROR: unable to open " << filename << endl;
    return false;
  }

  if (!BuildIndex())
    return false;

  if (index.size() == 0) {
    cerr << "ScorePlayer: File is empty\n";
    exit(0);
  }
  
  ::makeDummyFile();
  
  errno = 0;
  child_pid = fork();
  switch(child_pid) {
  case -1: // error
    cerr <<
      "ScorePlayer: ERROR: unable to create a new process to run VCG: errno = ";
    if (errno == EAGAIN) cerr << "EAGAIN\n";
    else
      if (errno == ENOMEM) cerr << "ENOMEM\n";
      else
	cerr << "--check yourselves--\n";
    exit(1);
    
  case 0: // child process
    if (!freopen("./vcg.errors", "w", stderr))
      cerr << "ScorePlayer: Unable to reassign stderr" << endl;
    if (!freopen("./vcg.errors", "w", stdout))
      cerr << "ScorePlayer: Unable to reassign stdout" << endl;
    // execlp("/project/cs/brass/a/tools/free/vcg.1.30/bin/xvcg",     
    execlp("xvcg", 
           "xvcg", "-a", "1", DUMMY_NAME, (char*) NULL);
    cerr << "ScorePlayer: ERROR: unable to exec xvcg" << endl;
    exit(1);
    
  default: // parent process
    cerr << "Initializing ..... please wait .....";
    sleep(5);
    cerr << "done" << endl;
    break;
  }

  currentFrame = 0;
  
  return true;
}

// make a file that vcg must open
void makeDummyFile ()
{
  FILE *fp;
  if (!(fp = fopen(DUMMY_NAME, "w"))) {
    cerr << "ScorePlayer: ERROR: unable to make dummy file" << endl;
    exit(1);
  }

  fprintf(fp, "graph: { \nnode: {\n title: \"CPU\" }\n }\n");

  fclose(fp);
}

bool ScorePlayer::BuildIndex()
{
  rewind(dataFile);
  index.clear();
  
  while (!feof(dataFile)) {
    PlayerIndexItem *item = new PlayerIndexItem;
    ScoreStateGraph graph(0);

    item->diskOffset = ftell(dataFile);

    int ret_val;

    if ((ret_val = graph.read(dataFile)) == READ_OK) {
      item->timeSlice = graph.getCurrentTimeslice();
      index.push_back(item);
    }
    else {
      if (ret_val == READ_EOF)
	return true;   // if eof, we're done
      else {
	cerr << "ScorePlayer: Error reading the file, it may be corrupted" << endl;
	return false;  // error was encountered
      }
    }
  }
  return true;
}


bool ScorePlayer::Go(int frame_number)
{
  if ((frame_number < 0) || ((unsigned int)frame_number > index.size()))
    return false;
  
  need_update = true;
  currentFrame = frame_number;
  return true;
}

bool ScorePlayer::Forward(int delta)
{
  if ((unsigned int)(currentFrame+delta) >= index.size())
    return false;
  
  need_update = true;
  currentFrame += delta;
  return true;
}

bool ScorePlayer::Back(int delta)
{
  if (currentFrame-delta < 0)
    return false;

  need_update = true;
  currentFrame -= delta;
  return true;
}

bool ScorePlayer::Search(unsigned int timeslice)
{
  for (unsigned int i = 0; i < index.size(); i ++) {
    if (index[i]->timeSlice == timeslice) {
      need_update = true;
      currentFrame = i;
      return true;
    }
    // since the timeslices are ordered, there is no chance to find one
    // if the following is true
    if (index[i]->timeSlice > timeslice)
      break;
  }

  return false;
}


void ScorePlayer::Display()
{
  if (!need_update)
    return;

  need_update = false;

  if (fseek(dataFile, index[currentFrame]->diskOffset, SEEK_SET)) {
    cerr << "ScorePlayer: ERROR: Unable to seek file\n";
    exit(1);
  }
  
  int ret_val;
  ScoreStateGraph graph(0);

  if ((ret_val = graph.read(dataFile)) != READ_OK) {
    cerr << "ScorePlayer: ERROR: Problems reading file, ret_val = " << 
      ret_val << endl;
    exit(1);
  }
  
  FILE *vcg_fp;
  
  if (!(vcg_fp = fopen(DUMMY_NAME, "w"))) {
    cerr << "ScorePlayer: Error opening " << DUMMY_NAME << endl;
    exit(1);
  }

  graph.writeVCG(vcg_fp, gSplitCPU, gNOCPU);
  fclose(vcg_fp);

  // make xvcg refresh
  kill (child_pid, SIGUSR1);
}

void ScorePlayer::Run()
{
  if (child_pid == 0)
    return;

  char command[100];
  bool quit = false;
  int i = 0;
  
  while (!quit) {
    Display();
    
    fprintf(stdout, "Frame %d (%u) > ", currentFrame,
	    index[currentFrame]->timeSlice);

    if (!fgets(command, 99, stdin))
      continue;
    
    switch(tolower(command[i])) {
    case 'q':
      quit = true;
      break;
    case 'g':
      {	int pos;
	if (parseArg(command+i+1, &pos)) 
	  Go(pos);
      }
      break;
    case 'f':
      {
	int delta;
	if(*(command+i+1) != '\n') {
	  if (parseArg(command+i+1, &delta))
	    Forward(delta);
	}
	else
	  Forward(1);
      }
      break;
    case '\n':
      Forward(1);
      break;
    case 'b':
      {	
	int delta;
	if(*(command+i+1) != '\n') {
	  if (parseArg(command+i+1, &delta))
	    Back(delta);
	}
	else
	  Back(1);
      }
      break;
    case 's':
      {	int slice;
	if (parseArg(command+i+1, &slice))
	  Search(slice);
      }
      break;
    case 'h':
      printHelpMsg();
      break;
    default:
      cerr << "Invalid command" << endl;
    }
  } 

  // this should close xcvg
  kill (child_pid, SIGUSR2);
  kill (child_pid, SIGKILL);
}

bool parseArg(char *str, int *number)
{
  char *endptr;
  
  *number = strtol(str, &endptr, 10);
  
  if (endptr <= str) {
    cerr << "ScorePlayer: ERROR: invalid argument" << endl;
    return false;
  }
  else
    return true;
} 


void ScorePlayer::printHelpMsg()
{
  static char *helpMsg =
    "q..........................Quit the ScorePlayer\n" \
    "g<frame_number>............Go to the specified frame number\n" \
    "f<increment>...............Forward by the specified number of frames (default 1)\n" \
    "b<decrement>...............Back by the specified number of frames (default 1)\n" \
    "s<slice_number>............Search for the frame with slice_number\n" \
    "'Enter key'................Equivalent to f1 (forward by 1 frame)\n" \
    "h..........................Print this help message\n";
  fprintf(stdout, helpMsg); 
}



