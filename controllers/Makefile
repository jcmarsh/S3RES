RESEARCH=/home/jcmarsh/research
SRC=../src/commtypes.c ../src/fd_client.c ../src/controller.c
INC=../include
LIB=-lrt -lTASLimited
#FLAGS=-O3
FLAGS=-g -O0 -Werror

all: ArtPot AStar BenchMarker Empty Filter Logger Mapper VoterD

%.o: %.c
	gcc -c $(FLAGS) -o $@ $< 

ArtPot: art_pot.o $(SRC) $(INC)/*.h
	gcc $(FLAGS) -o ArtPot art_pot.o $(SRC) $(LIB) -lm

AStar: a_star.o ../src/mapping.c $(SRC) $(INC)/*.h
	gcc $(FLAGS) -o AStar a_star.o ../src/mapping.c $(SRC) $(LIB) -lm

BenchMarker:  benchmarker.o ../src/commtypes.c ../src/replicas.c $(INC)/*.h
	gcc $(FLAGS) -o BenchMarker benchmarker.o ../src/commtypes.c ../src/replicas.c $(LIB)

Empty: empty.o $(SRC) $(INC)/*.h
	gcc $(FLAGS) -o Empty empty.o $(SRC) $(LIB) 

Filter: filter.o $(SRC) $(INC)/*.h
	gcc $(FLAGS) -o Filter filter.o $(SRC) $(LIB) 

Logger: logger.o $(SRC) $(INC)/*.h
	gcc $(FLAGS) -o Logger logger.o $(SRC) $(LIB) -lm

PassThrough: pass_through.o
	gcc $(FLAGS) -o PassThrough pass_through.o $(LIB)  

Mapper: mapper.o ../src/mapping.c $(SRC) $(INC)/*.h
	gcc $(FLAGS) -o Mapper mapper.o ../src/mapping.c $(SRC) $(LIB) -lm

VoterD: voterd.o ../src/commtypes.c ../src/replicas.c ../src/fd_server.c ../src/fd_client.c $(INC)/*.h
	gcc $(FLAGS) -o VoterD voterd.o ../src/commtypes.c ../src/replicas.c ../src/fd_server.c ../src/fd_client.c $(LIB)

Min: min.o ../src/commtypes.c ../src/replicas.c $(INC)/*.h
	gcc $(FLAGS) -o Min min.o ../src/commtypes.c ../src/replicas.c

copy:
	cp ArtPot ../../stage/experiments/
	cp AStar ../../stage/experiments/
	cp BenchMarker ../../stage/experiments/
	cp Empty ../../stage/experiments/
	cp Filter ../../stage/experiments/
	cp Logger ../../stage/experiments/
	cp Mapper ../../stage/experiments/
	cp VoterD ../../stage/experiments/

clean:
	rm -f *.o
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
