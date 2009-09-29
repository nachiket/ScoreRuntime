#include "ScoreStream.h"
#include <iostream>
#include <cstdio>
#include <cstdlib>

#include "tb.h"

using namespace std;

int main(int argc, char *argv[]) {

    score_init();

    BOOLEAN_SCORE_STREAM  fire = NEW_BOOLEAN_SCORE_STREAM();
    BOOLEAN_SCORE_STREAM  fired = NEW_BOOLEAN_SCORE_STREAM();
    //BOOLEAN_SCORE_STREAM  fired;
    
    NEW_tb(fire, fired);

    for(int i=0; i<6;i++) {
    	cout << "Fire signal at i=" << i << endl;
    	STREAM_WRITE(fire, false);
    	STREAM_WRITE(fire, false);
    	STREAM_WRITE(fire, false);
    	STREAM_WRITE(fire, true);
    	STREAM_WRITE(fire, true);
    	STREAM_WRITE(fire, true);
    	STREAM_WRITE(fire, true);
    	STREAM_WRITE(fire, true);

	unsigned char byte;
	while(!STREAM_EMPTY(fired)) {
		STREAM_READ(fired, byte);
		cout << "Byte=" << (int)byte << endl;
	}
    }

    STREAM_CLOSE(fire);
    STREAM_FREE(fired);

    score_exit();
}

