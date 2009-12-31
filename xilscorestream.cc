#include <pthread.h>
#include "xilxcorenode.h"
#include "xilscorestream.h"

#define sched_yield yield

void *ScoreStream::operator new(size_t size) {

	void *ptr = 0;

	// create a memory region for use as the buffer..
	ptr = malloc(size);
	if (!ptr) {
		perror("ptr -- stream new operator -- unable to malloc mem");
		exit(errno);
	}

	return ptr;
}

void ScoreStream::operator delete(void *rawMem, size_t size)
{
	ScoreStream *strm = (ScoreStream *)rawMem;
	free(rawMem);
}


ScoreStream::ScoreStream() {}


ScoreStream::ScoreStream(int width_t, int fixed_t, int length_t,
		ScoreType type_t,
		unsigned int user_stream_type,
		int depth_hint_t) {


	assert(length_t == ARRAY_FIFO_SIZE);

	// constructor for ScoreStream class
	width = width_t;
	fixed = fixed_t;
	depth_hint = depth_hint_t;
	length = length_t;
	type = type_t;

	// crucial for correct operation
	head = tail = 0; 
	token_written = token_read = token_eos = 0;
	src = sink = NULL;

	// Create a mutex for this stream  
	int ret=pthread_mutex_init(&mutex, NULL);
	if(ret==EAGAIN) {
		xil_printf("mutex -- init -- creating failure out of resources");
		exit(1);
	}

	producerClosed = 0;
	consumerFreed = 0;

	srcIsDone = 0;
	sinkIsDone = 0;

	srcFunc = STREAM_OPERATOR_TYPE;
	snkFunc = STREAM_OPERATOR_TYPE;


}

ScoreStream::~ScoreStream()
{
	if (VERBOSEDEBUG || DEBUG) {
		cerr << "[SID=" << streamID << "][" << (unsigned int) this << "] "
			<< "ScoreStream Destructor called on " << streamID << endl;
	}

	pthread_mutex_destroy(&mutex);

}

long long int ScoreStream::stream_read() {

	long long int local_buffer;

	// if stream is freed, disallow any more reads.
	if (consumerFreed) {
		cerr << "xilscorestream.h: Attempting to read from a freed stream!" << endl;
		exit(1);
	}

	// check to see if empty
	while (head == tail) {
		sched_yield();
	}

	// core read operation
	pthread_mutex_lock(&mutex);
	local_buffer = buffer[head].token;
	head = (head+1) % (length+1+1);
	token_read++;
	pthread_mutex_unlock(&mutex);

	if (VERBOSEDEBUG || DEBUG) {
		printf("[SID=%d]   Stream Read: %llx \n", streamID, local_buffer);

	}

	return local_buffer;
}

void ScoreStream::stream_write(long long int input, int writingEOS) {

	// if the producerClosed, then disallow any more writes!
	if (producerClosed && !writingEOS) {
		cerr << "xilscorestream.h: Attempting to write a closed stream [SID=" << streamID << endl;
		exit(1);
	}

	// if the sinkIsDone, then just complete since all tokens are being thrown away!
	if (sinkIsDone) {
		if (VERBOSE) {
			cout << "[SID=" << streamID << "]  Throwing away written token since sinkIsDone!" << endl;
		}

		return;
	}

	// check to see if full
	while (((head - tail) == 1) || 
			((tail == ((length+1+1)-1) && (head == 0)))) {
		sched_yield();
	}

	pthread_mutex_lock(&pthread);
	// core write operation
	buffer[tail].token = input;
	buffer[tail].timeStamp = cTime;
	tail = (tail+1) % (length+1+1);
	token_written++;
	pthread_mutex_unlock(&pthread);

	if (VERBOSE) {
		printf("[SID=%d]   Stream Write: %llx \n", streamID,input);
	}

}

// TODO: add datatype specific eos.
int ScoreStream::stream_eos() {

	int isEOS = 0;

	token_eos++;

	long long int local_buffer;

	if (VERBOSE)
		cout << "[SID=" << streamID << "]  entering stream_eos \n";

	// check empty
	while (head == tail) {
		sched_yield();
	}

	// check
	pthread_mutex_lock(&mutex);
	if (producerClosed && (get_numtokens() == 1)) {
		local_buffer = buffer[head].token;

		if (local_buffer == (long long int)EOS) {
			isEOS = 1;
		} else {
			isEOS = 0;
		}
	}
	pthread_mutex_unlock(&mutex);

	return(isEOS);
}

// End-of-Frame signal added
// TODO: Add datatype specific EOFR signaling.. How about a special bool flag itself? This would be so awkward on a Microblaze!
int ScoreStream::stream_eofr() {

	int isEOFR = 0;

	token_eofr++;

	// make sure there is a token in the stream
	// and exam it to see if it is a EOF token
	// otherwise block

	long long int local_buffer;

	if (VERBOSE)
		cout << "[SID=" << streamID << "]  entering stream_eos \n";

	while (head == tail) {
		sched_yield();
	}

	// check
	pthread_mutex_lock(&mutex);
	if (get_numtokens() >= 1) {
		local_buffer = buffer[head].token;

		// check the token value to make sure it is the EOF token.
		if (local_buffer == (long long int)EOFR) {
			isEOFR = 1;
		} else {
			isEOFR = 0;
		}
	}
	pthread_mutex_unlock(&mutex);

	return(isEOFR);
}

void stream_close(ScoreStream *strm) {

	if (VERBOSE) 
		cerr << "[SID=" << strm->streamID << "]   entering stream_close" << endl;

	// make sure this is not a double close.
	if (strm->producerClosed) {
		cerr << "xilscorestream.h Error: Trying to double-close a stream!" << srtm->streaID << endl;
		exit(1);
	}

	pthread_mutex_lock(&mutex);
	strm->producerClosed = 1;
	pthread_mutex_unlock(&mutex);

	strm->stream_write(EOS, 1);
}

void stream_frame_close(ScoreStream *strm) {

	if (VERBOSE)
		cerr << "[SID=" << strm->streamID << "]   entering stream_frame_close" << endl;

	// need to write a EOFR token to the stream
	strm->stream_write(EOFR, 1);
}


void stream_free(ScoreStream *strm) {

	if (VERBOSE) {
		cerr << "[SID=" << strm->streamID << "]   entering stream_free" << endl;
	}

	// make sure this is not a double free.
	if (strm->consumerFreed) {
		cerr << "xilscorestream.h Error: Trying to double-free a stream!" << srtm->streaID << endl;
		exit(1);
	}

	pthread_mutex_lock(&mutex);    
	strm->consumerFreed = 1;
	pthread_mutex_unlock(&mutex);    

}

