#include "ScoreStream.h"
#include <iostream>
#include <cstdio>
#include <cstdlib>

#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#include "ab.h"

// Copied from ScoreRuntime.cc
// even though msgbuf is defined in msg.h, we want to have a message with
// a longer text section, so we redefine it here.
typedef struct rmsgbuf {
  long mtype;
  char mtext[4080];
} rmsgbuf;

using namespace std;

DOUBLE_SCORE_STREAM receive_stream;
DOUBLE_SCORE_STREAM send_stream;
DOUBLE_SCORE_STREAM receive_stream1;
DOUBLE_SCORE_STREAM send_stream1;

int main(int argc, char *argv[]) {

	score_init();

	int i=0;

	// Receive stream ids for the shared stream structures...
	rmsgbuf *msgp = new struct rmsgbuf();
	if (msgp == NULL) {
		cerr << "Insufficient memory to instantiate IPC buffer!" << endl;
		exit(1);
	}

	// Setup IPC
	key_t ipcKey = ftok(".", 0);
	if (ipcKey == -1) {
		cerr << "b.cc: ipc key error!" << endl;
		exit(1);
	}
	int ipcID = msgget(ipcKey, IPC_CREAT|0666);
	cout << "b.cc: Id=" << ipcID << endl;

	// go into a loop to get messages from a.cc
	int len;
	char *argbuf;
	bool ctrl_pkt_received=false;
	while (!ctrl_pkt_received) {
		int msgsz = msgrcv(ipcID, msgp, sizeof(rmsgbuf), SCORE_INSTANTIATE_MESSAGE_TYPE, 0);

		// process the message.
		if (msgsz == -1) {
			cerr << "b.cc: Errno = " << errno << endl;
			exit(-1);
		} else {

			//msgctl(ipcID, IPC_STAT, &queueStat);

			// get the components from the message.
			memcpy(&len, msgp->mtext, 4);
			argbuf = new char[len];
			memcpy(argbuf, msgp->mtext+4, len);

			cout << "b.cc: Received ctrl pkt." << endl;
			ctrl_pkt_received=true;
		}
	}

	stream_arg *data;
	data=(stream_arg *)malloc(sizeof(stream_arg));
	memcpy(data,argbuf,sizeof(stream_arg));

	// initialize the stream object pointers
	cout << "b.cc: receive_stream_id=" << data->send_stream_id << endl;
        cout << "b.cc: send_stream_id=" << data->receive_stream_id << endl;
	cout << "b.cc: receive_stream1_id=" << data->send_stream1_id << endl;
        cout << "b.cc: send_stream1_id=" << data->receive_stream1_id << endl;
	receive_stream = (DOUBLE_SCORE_STREAM)STREAM_ID_TO_OBJ(data->send_stream_id);
	send_stream = (DOUBLE_SCORE_STREAM)STREAM_ID_TO_OBJ(data->receive_stream_id);
	receive_stream1 = (DOUBLE_SCORE_STREAM)STREAM_ID_TO_OBJ(data->send_stream1_id);
	send_stream1 = (DOUBLE_SCORE_STREAM)STREAM_ID_TO_OBJ(data->receive_stream1_id);

	// Receive stream
	for(i=0;i<10;i++) {
		double j=STREAM_READ_DOUBLE(receive_stream);
		cout << "b.cc: RECEIVE i=" << i << ", j=" << j << endl;
	}
	
	// Send stream
	for(i=0;i<=10;i++) {
		cout << "b.cc: SEND i=" << i << endl;
		STREAM_WRITE_DOUBLE(send_stream, (double)i+1);
	}

	//bool eos=STREAM_EOS(receive_stream);
	//STREAM_CLOSE(receive_stream);
	//STREAM_CLOSE(send_stream);

	score_exit();
}

