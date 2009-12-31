#include "ScoreStream.h"
#include "Score.h"
#include <iostream>
#include <cstdio>
#include <cstdlib>

#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#include "ab.h"

using namespace std;

int main(int argc, char *argv[]) {

	score_init();

	int i=0;

	DOUBLE_SCORE_STREAM send_stream = NEW_DOUBLE_SCORE_STREAM();
	DOUBLE_SCORE_STREAM receive_stream = NEW_DOUBLE_SCORE_STREAM();
	DOUBLE_SCORE_STREAM send_stream1 = NEW_DOUBLE_SCORE_STREAM();
	DOUBLE_SCORE_STREAM receive_stream1 = NEW_DOUBLE_SCORE_STREAM();

	// Setup IPC
	key_t ipcKey = ftok(".", 0);
	if (ipcKey == -1) {
		cerr << "a.cc: ipc key error!" << endl;
		exit(1);
	}
	int ipcID = msgget(ipcKey, IPC_CREAT|0666);
	cout << "a.cc: Id=" << ipcID << endl;

	// Construct a message with stream object ids..
	stream_arg *data;
	struct msgbuf *msgp;
	data=(stream_arg *)malloc(sizeof(stream_arg));
	data->send_stream_id=STREAM_OBJ_TO_ID(send_stream);
	data->receive_stream_id=STREAM_OBJ_TO_ID(receive_stream);
	data->send_stream1_id=STREAM_OBJ_TO_ID(send_stream1);
	data->receive_stream1_id=STREAM_OBJ_TO_ID(receive_stream1);
	cout << "a.cc: send_stream_id=" << data->send_stream_id << endl;
	cout << "a.cc: receive_stream_id=" << data->receive_stream_id << endl;;
	cout << "a.cc: send_stream1_id=" << data->send_stream1_id << endl;
	cout << "a.cc: receive_stream1_id=" << data->receive_stream1_id << endl;;
	int len=sizeof(stream_arg);
	msgp=(struct msgbuf *)malloc(sizeof(msgbuf)+sizeof(char)*(len+sizeof(long)-1));
	cout << "a.cc: schedulerid=" << ipcID << endl;
	memcpy(msgp->mtext,&len,sizeof(long));
	memcpy(msgp->mtext+sizeof(long),data,len);
	msgp->mtype=SCORE_INSTANTIATE_MESSAGE_TYPE;
	int res=msgsnd(ipcID, msgp, len+sizeof(long), 0) ;
	if (res==-1) {
		cerr <<"a.cc: errno=" << errno << endl;
		exit(2);    
	}

	// Send tokens
	for(i=0;i<10;i++) {
		cout << "a.cc: SEND i=" << i << endl;
		STREAM_WRITE_DOUBLE(send_stream, (double)i);
	}
	

	// Receive tokens
	for(i=0;i<10;i++) {
		double j=STREAM_READ_DOUBLE(receive_stream);
		cout << "a.cc: RECEIVE i=" << i << ", j=" << j << endl;
	}
	
	STREAM_CLOSE(send_stream);
	STREAM_CLOSE(receive_stream);

	score_exit();
}

