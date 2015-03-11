RESEARCH=/home/jcmarsh/research
SRC=../src/commtypes.c ../src/fd_client.c ../src/controller.c
LIB=-lrt -lTASLimited
FLAGS=-O3
#FLAGS=-g -O0 -Werror

all: regular

regular:
	gcc $(FLAGS) -o ArtPot art_pot.c  $(SRC) $(LIB) -lm
	gcc $(FLAGS) -o AStar a_star.c $(SRC) ../src/mapping.c $(LIB) -lm
	gcc $(FLAGS) -o BenchMarker benchmarker.c ../src/commtypes.c ../src/replicas.c $(LIB)
	gcc $(FLAGS) -o Empty empty.c $(SRC) $(LIB) 
	gcc $(FLAGS) -o Filter filter.c $(SRC) $(LIB) 
	gcc $(FLAGS) -o PassThrough pass_through.c $(LIB) 
	gcc $(FLAGS) -o Mapper mapper.c $(SRC) ../src/mapping.c $(LIB) -lm
	gcc $(FLAGS) -o VoterD voterd.c ../src/commtypes.c ../src/replicas.c ../src/fd_server.c ../src/fd_client.c $(LIB)
	gcc $(FLAGS) -o Min min.c ../src/commtypes.c ../src/replicas.c

copy:
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
