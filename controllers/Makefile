RESEARCH=/home/jcmarsh/research
SRC=../src/commtypes.cpp ../src/fd_client.cpp
LIB=-lrt -lTASLimited
FLAGS=-O3
#FLAGS=-g -O0

all: regular

regular:
	g++ $(FLAGS) -o ArtPot art_pot.c $(SRC) $(LIB)
	g++ $(FLAGS) -o AStar a_star.c $(SRC) ../src/mapping.cpp $(LIB)
	g++ $(FLAGS) -o BenchMarker benchmarker.c ../src/commtypes.cpp ../src/replicas.cpp $(LIB)
	g++ $(FLAGS) -o Empty empty.c $(SRC) $(LIB) 
	g++ $(FLAGS) -o Filter filter.c $(SRC) $(LIB) 
	g++ $(FLAGS) -o PassThrough pass_through.c $(SRC) $(LIB) 
	g++ $(FLAGS) -o Mapper mapper.c $(SRC) ../src/mapping.cpp $(LIB) 
	g++ $(FLAGS) -o VoterC voterc.c ../src/commtypes.cpp ../src/replicas.cpp ../src/fd_server.cpp ../src/fd_client.cpp $(LIB)
	g++ $(FLAGS) -o Min min.c ../src/commtypes.cpp ../src/replicas.cpp

copy: regular
	cp ArtPot ../../stage/experiments/
	cp AStar ../../stage/experiments/
	cp BenchMarker ../../stage/experiments/
	cp Empty ../../stage/experiments/
	cp Filter ../../stage/experiments/
	cp PassThrough ../../stage/experiments/
	cp Mapper ../../stage/experiments/
	cp VoterC ../../stage/experiments/

test: regular
	cp ArtPot ../test/
	cp AStar ../test/
	cp BenchMarker ../test/
	cp Empty ../test/
	cp Filter ../test/
	cp PassThrough ../test/
	cp Mapper ../test/
	cp VoterC ../test/
	cp Min ../test/

clean:
	rm -f ArtPot
	rm -f AStar
	rm -f BenchMarker
	rm -f Empty
	rm -f Filter
	rm -f PassThrough
	rm -f Mapper
	rm -f VoterC
	rm -f Min
	rm -f ../test/ArtPot
	rm -f ../test/AStar
	rm -f ../test/BenchMarker
	rm -f ../test/Empty
	rm -f ../test/Filter
	rm -f ../test/PassThrough
	rm -f ../test/Mapper
	rm -f ../test/VoterC
	rm -f ../test/Min
