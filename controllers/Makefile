SRC=../src/commtypes.c ../src/fd_client.c ../src/controller.c
DEP=../include/*.h ../tas_lib/inc/*.h
INC=-I../include/ -I../tas_lib/inc/
LIB=-lrt -lTASLimited
FLAGS=-O3 -Werror -static
#FLAGS=-g -O0 -Werror -static

all: ArtPot AStar BenchMarker Empty Filter Logger Mapper

%.o: %.c
	gcc -c $(FLAGS) -o $@ $< -I../include/ -I../tas_lib/inc/

ArtPot: art_pot.o $(SRC) $(DEP)
	gcc $(FLAGS) -o ArtPot art_pot.o $(SRC) $(INC) $(LIB) -lm

AStar: a_star.o mapping.c $(SRC) $(DEP) ./inc/*.h
	gcc $(FLAGS) -o AStar a_star.o mapping.c $(SRC) $(INC) $(LIB) -lm

BenchMarker: benchmarker.o ../src/commtypes.c ../src/replicas.c $(DEP)
	gcc $(FLAGS) -o BenchMarker benchmarker.o ../src/commtypes.c ../src/fd_server.c ../src/replicas.c $(INC) $(LIB)

Empty: empty.o $(SRC) $(DEP)
	gcc $(FLAGS) -o Empty empty.o $(SRC) $(INC) $(LIB) 

Filter: filter.o $(SRC) $(DEP)
	gcc $(FLAGS) -o Filter filter.o $(SRC) $(INC) $(LIB) 

Logger: logger.o $(SRC) $(DEP)
	gcc $(FLAGS) -o Logger logger.o $(SRC) $(INC) $(LIB) -lm

Mapper: mapper.o mapping.c $(SRC) $(DEP) ./inc/*.h
	gcc $(FLAGS) -o Mapper mapper.o mapping.c $(SRC) $(INC) $(LIB) -lm

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
