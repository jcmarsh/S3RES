include ../directory_paths

SRC=../src/commtypes.c ../src/fd_client.c ../src/controller.c
DEP=../include/*.h ../tas_lib/inc/*.h
INC=-I../include/ -I../tas_lib/inc/
LIB=-L/usr/local/lib -lrt -lTASLimited -pthread
COMPILE=/usr/local/musl/bin/musl-gcc -Werror -static
#COMPILE=gcc -O3 -Werror -static
FLAGS=-O3 -Werror -static
#FLAGS=-g -O0 -Werror -static

all: ArtPot AStar BenchMarker Empty GenericEmpty Filter Logger Mapper Load

%.o: %.c
	$(COMPILE) -c -o $@ $< -I../include/ -I../tas_lib/inc/

ArtPot: art_pot.o $(SRC) $(DEP)
	$(COMPILE) -o ArtPot art_pot.o $(SRC) $(INC) $(LIB) -lm

AStar: a_star.o mapping.c $(SRC) $(DEP) ./inc/*.h
	$(COMPILE) -o AStar a_star.o mapping.c $(SRC) $(INC) $(LIB) -lm

BenchMarker: benchmarker.o ../src/commtypes.c ../src/replicas.c ../voter/vote_buff.c $(DEP)
	$(COMPILE) -o BenchMarker benchmarker.o ../src/commtypes.c ../src/fd_server.c ../src/replicas.c ../voter/vote_buff.c $(INC) $(LIB)

Empty: empty.o $(SRC) $(DEP)
	$(COMPILE) -o Empty empty.o $(SRC) $(INC) $(LIB)

Filter: filter.o $(SRC) $(DEP)
	$(COMPILE) -o Filter filter.o $(SRC) $(INC) $(LIB)

Logger: logger.o $(SRC) $(DEP)
	$(COMPILE) -o Logger logger.o $(SRC) $(INC) $(LIB) -lm

Mapper: mapper.o mapping.c $(SRC) $(DEP) ./inc/*.h
	$(COMPILE) -o Mapper mapper.o mapping.c $(SRC) $(INC) $(LIB) -lm

Load: load.c $(SRC) $(DEP)
	gcc -c $(FLAGS) -o load.o load.c -I../include/ -I../tas_lib/inc/
	gcc -O3 -Werror -L"~/research/ccv/lib" -I"~/research/ccv/lib" -I"/usr/include" -o Load load.o $(SRC) $(INC) $(LIB) -lccv `cat ~/research/ccv/lib/.deps`

GenericEmpty: generic_empty.o $(SRC) $(DEP)
	$(COMPILE) -o GenericEmpty generic_empty.o $(SRC) $(INC) $(LIB)

copy:
	cp ArtPot $(INSTALL_DIR)
	cp AStar $(INSTALL_DIR)
	cp BenchMarker $(INSTALL_DIR)
	cp Empty $(INSTALL_DIR)
	cp Filter $(INSTALL_DIR)
	cp Logger $(INSTALL_DIR)
	cp Mapper $(INSTALL_DIR)
	cp Load $(INSTALL_DIR)

clean:
	rm -f *.o
	rm -f ArtPot
	rm -f AStar
	rm -f BenchMarker
	rm -f Empty
	rm -f GenericEmpty
	rm -f Filter
	rm -f Logger
	rm -f Mapper
	rm -f Load
