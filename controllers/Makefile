RESEARCH=/home/jcmarsh/research
SRC=../src/commtypes.cpp
LIB=-lrt -lTASLimited
FLAGS=-O3

all: regular

regular:
	g++ -o pb pb.c
	g++ $(FLAGS) -o ArtPot art_pot_p.c $(SRC) ../src/fd_client.cpp $(LIB)
#	g++ $(FLAGS) -o ArtPot a_star.c $(SRC) ../src/fd_client.cpp $(LIB)
	g++ $(FLAGS) -o Empty empty.c $(SRC) ../src/fd_client.cpp $(LIB) 
	g++ $(FLAGS) -o VoterB voterb.c $(SRC) ../src/replicas.cpp ../src/fd_server.cpp $(LIB)
#	g++ $(FLAGS) -o VoterC voterc.c $(SRC) ../src/replicas.cpp ../src/fd_server.cpp $(LIB)
	g++ $(FLAGS) -o BenchMarker benchmarker.c $(SRC) ../src/replicas.cpp $(LIB)

copy: regular
	cp pb ../../stage/experiments/
	cp ArtPot ../../stage/experiments/
#	cp AStar ../../stage/experiments/
	cp Empty ../../stage/experiments/
	cp VoterB ../../stage/experiments/
#	cp VoterC ../../stage/experiments/
	cp BenchMarker ../../stage/experiments/

clean:
	rm -f pb
	rm -f ArtPot
#	rm -f AStar
	rm -f Empty
	rm -f VoterB
#	rm -f VoterC
	rm -f BenchMarker
