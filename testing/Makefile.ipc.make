all:	a.o b.o
a.o:	a.cc ../libScoreRuntime.a
	g++ -Wno-write-strings -I/opt/leda/incl -I/home/nachiket/workspace/ScoreRuntime -L/home/nachiket/workspace/ScoreRuntime -L/opt/leda a.cc -o a.o -lleda -lX11 -lScoreRuntime -lpthread
b.o:	b.cc ../libScoreRuntime.a
	g++ -Wno-write-strings -I/opt/leda/incl -I/home/nachiket/workspace/ScoreRuntime -L/home/nachiket/workspace/ScoreRuntime -L/opt/leda b.cc -o b.o -lleda -lX11 -lScoreRuntime -lpthread

