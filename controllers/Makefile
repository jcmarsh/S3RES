RESEARCH=/home/jcmarsh/research
SRC=../src/commtypes.cpp ../src/fd_client.cpp ../src/controller.c
LIB=-lrt -lTASLimited
#FLAGS=-O3
FLAGS=-g -O0

all: regular

regular:
	g++ $(FLAGS) -o ArtPot art_pot.c  $(SRC) $(LIB)
	g++ $(FLAGS) -o AStar a_star.c $(SRC) ../src/mapping.cpp $(LIB)
	g++ $(FLAGS) -o BenchMarker benchmarker.c ../src/commtypes.cpp ../src/replicas.cpp $(LIB)
	g++ $(FLAGS) -o Empty empty.c $(SRC) $(LIB) 
	g++ $(FLAGS) -o Filter filter.c $(SRC) $(LIB) 
	g++ $(FLAGS) -o PassThrough pass_through.c $(LIB) 
	g++ $(FLAGS) -o Mapper mapper.c $(SRC) ../src/mapping.cpp $(LIB) 
	g++ $(FLAGS) -o VoterD voterd.c ../src/commtypes.cpp ../src/replicas.cpp ../src/fd_server.cpp ../src/fd_client.cpp $(LIB)
	g++ $(FLAGS) -o Min min.c ../src/commtypes.cpp ../src/replicas.cpp

copy: regular
	cp ArtPot ../../stage/experiments/
	cp AStar ../../stage/experiments/
	cp BenchMarker ../../stage/experiments/
	cp Empty ../../stage/experiments/
	cp Filter ../../stage/experiments/
	cp PassThrough ../../stage/experiments/
	cp Mapper ../../stage/experiments/
	cp VoterD ../../stage/experiments/

clean:
	rm -f ArtPot
	rm -f AStar
	rm -f BenchMarker
	rm -f Empty
	rm -f Filter
	rm -f PassThrough
	rm -f Mapper
	rm -f VoterD
	rm -f Min
