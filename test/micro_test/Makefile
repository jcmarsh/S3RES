SRC=../../src/commtypes.c ../../src/replicas.c ../../src/fd_server.c ../../voter/vote_buff.c
INC=-I../../include -I../../tas_lib/inc

all: Empty EmptyBench EmptyRestart

%.o: %.c
	gcc -c -O3 -o $@ $< $(INC)

Empty:
	make -C ../../controllers

EmptyBench: empty_bench.o
	gcc -O3 -o EmptyBench empty_bench.o $(SRC) -lrt -lTASLimited
	cp ../../controllers/Empty ./

EmptyRestart: empty_restart.o $(SRC)
	gcc -O3 -o EmptyRestart empty_restart.o $(SRC) -lrt -lTASLimited
	cp ../../controllers/Empty ./

clean:
	rm -f *.o
	rm -rf Empty
	rm -rf EmptyBench
	rm -rf EmptyRestart
	rm -rf Empty_fd_server
