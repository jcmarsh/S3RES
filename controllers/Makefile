RESEARCH=/home/jcmarsh/research
SRC=../src/commtypes.c ../src/fd_client.c ../src/controller.c
LIB=-lrt -lTASLimited
FLAGS=-O3
#FLAGS=-g -O0 -Werror

all: ArtPot AStar BenchMarker Empty Filter Logger PassThrough Mapper VoterD Min

%.o: %.c
	gcc -c $(FLAGS) -o $@ $< 

ArtPot: art_pot.o $(SRC)
	gcc $(FLAGS) -o ArtPot art_pot.o $(SRC) $(LIB) -lm

AStar: a_star.o ../src/mapping.c $(SRC)
	gcc $(FLAGS) -o AStar a_star.o ../src/mapping.c $(SRC) $(LIB) -lm

BenchMarker:  benchmarker.o ../src/commtypes.c ../src/replicas.c
	gcc $(FLAGS) -o BenchMarker benchmarker.o ../src/commtypes.c ../src/replicas.c $(LIB)

Empty: empty.o $(SRC)
	gcc $(FLAGS) -o Empty empty.o $(SRC) $(LIB) 

Filter: filter.o $(SRC)
	gcc $(FLAGS) -o Filter filter.o $(SRC) $(LIB) 

Logger: logger.o $(SRC)
	gcc $(FLAGS) -o Logger logger.o $(SRC) $(LIB) -lm

PassThrough: pass_through.o
	gcc $(FLAGS) -o PassThrough pass_through.o $(LIB)  

Mapper: mapper.o ../src/mapping.c $(SRC)
	gcc $(FLAGS) -o Mapper mapper.o ../src/mapping.c $(SRC) $(LIB) -lm

VoterD: voterd.o ../src/commtypes.c ../src/replicas.c ../src/fd_server.c ../src/fd_client.c
	gcc $(FLAGS) -o VoterD voterd.o ../src/commtypes.c ../src/replicas.c ../src/fd_server.c ../src/fd_client.c $(LIB)

Min: min.o ../src/commtypes.c ../src/replicas.c
	gcc $(FLAGS) -o Min min.o ../src/commtypes.c ../src/replicas.c

copy:
	cp ArtPot ../../stage/experiments/
	cp AStar ../../stage/experiments/
	cp BenchMarker ../../stage/experiments/
	cp Empty ../../stage/experiments/
	cp Filter ../../stage/experiments/
	cp Logger ../../stage/experiments/
	cp PassThrough ../../stage/experiments/
	cp Mapper ../../stage/experiments/
	cp VoterD ../../stage/experiments/

clean:
	rm *.o
	rm -f ArtPot
	rm -f AStar
	rm -f BenchMarker
	rm -f Empty
	rm -f Filter
	rm -f Logger
	rm -f PassThrough
	rm -f Mapper
	rm -f VoterD
	rm -f Min
