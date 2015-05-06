SRC=../src/commtypes.c ../src/fd_client.c ../src/controller.c
INC=../include/*.h
LIB=-lrt -lTASLimited
FLAGS=-O3 -static
#FLAGS=-g -O0 -Werror -static

all: ArtPot AStar BenchMarker Empty Filter Logger Mapper

%.o: %.c
	gcc -c $(FLAGS) -o $@ $< 

ArtPot: art_pot.o $(SRC) $(INC)
	gcc $(FLAGS) -o ArtPot art_pot.o $(SRC) $(LIB) -lm

AStar: a_star.o ../src/mapping.c $(SRC) $(INC)
	gcc $(FLAGS) -o AStar a_star.o ../src/mapping.c $(SRC) $(LIB) -lm

BenchMarker: benchmarker.o ../src/commtypes.c ../src/fd_server.c ../src/replicas.c $(INC)
	gcc $(FLAGS) -o BenchMarker benchmarker.o ../src/commtypes.c ../src/fd_server.c ../src/replicas.c $(LIB)

Empty: empty.o $(SRC) $(INC)
	gcc $(FLAGS) -o Empty empty.o $(SRC) $(LIB) 

Filter: filter.o $(SRC) $(INC)
	gcc $(FLAGS) -o Filter filter.o $(SRC) $(LIB) 

Logger: logger.o $(SRC) $(INC)
	gcc $(FLAGS) -o Logger logger.o $(SRC) $(LIB) -lm

Mapper: mapper.o ../src/mapping.c $(SRC) $(INC)
	gcc $(FLAGS) -o Mapper mapper.o ../src/mapping.c $(SRC) $(LIB) -lm

copy:
	cp ArtPot ../../stage/experiments/
	cp AStar ../../stage/experiments/
	cp BenchMarker ../../stage/experiments/
	cp Empty ../../stage/experiments/
	cp Filter ../../stage/experiments/
	cp Logger ../../stage/experiments/
	cp Mapper ../../stage/experiments/

clean:
	rm -f *.o
	rm -f ArtPot
	rm -f AStar
	rm -f BenchMarker
	rm -f Empty
	rm -f Filter
	rm -f Logger
	rm -f Mapper
