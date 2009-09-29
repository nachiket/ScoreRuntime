all:	add.o add8.cc add8.h tb.o tb.cc tb.h
add.o:	add.C ../libScoreRuntime.a add8.cc add8.h add.tdf
	g++ -I/opt/leda/incl -I/home/nachiket/workspace/ScoreRuntime -L/home/nachiket/workspace/ScoreRuntime -L/opt/leda add.C add8.cc -o add.o -lleda -lX11 -lScoreRuntime -lpthread
add8.cc add8.h:	add.tdf
	tdfc -ecc add.tdf
tb.o:	tb.C ../libScoreRuntime.a tb.cc tb.h tb.tdf
	g++ -I/opt/leda/incl -I/home/nachiket/workspace/ScoreRuntime -L/home/nachiket/workspace/ScoreRuntime -L/opt/leda tb.C tb.cc -o tb.o -lleda -lX11 -lScoreRuntime -lpthread
tb.cc tb.h:	tb.tdf
	tdfc -ecc tb.tdf
clean:
	rm add.o add8.cc add8.h add.cc add.h tb.o tb.cc tb.h
