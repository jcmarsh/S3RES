RESEARCH=/home/jcmarsh/research
SRC=../src/commtypes.cpp
LIB=-lrt -lTASLimited
FLAGS=-O3

all: regular

regular:
	g++ -o ArtPot art_pot_p.c $(SRC) ../src/fd_client.cpp $(LIB)
	g++ -o Empty empty.c $(SRC) ../src/fd_client.cpp $(LIB) 
	g++ -o VoterB voterb.c $(SRC) ../src/replicas.cpp ../src/fd_server.cpp $(LIB)
	g++ -o BenchMarker benchmarker.c $(SRC) ../src/replicas.cpp $(LIB)

copy: regular
	cp ArtPot ../../stage/experiments/
	cp Empty ../../stage/experiments/
	cp VoterB ../../stage/experiments/
	cp BenchMarker ../../stage/experiments/

clean:
	rm -f ArtPot
	rm -f Empty
	rm -f VoterB
	rm -f BenchMarker
