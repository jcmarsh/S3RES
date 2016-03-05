SRC=../../src/commtypes.c ../../src/replicas.c ../../src/fd_server.c ../../voter/vote_buff.c
INC=-I../../include -I../../tas_lib/inc

all: Empty EmptyRestart

%.o: %.c
	gcc -c -O3 -o $@ $< $(INC)

Empty:
	make -C ../../controllers

EmptyRestart: empty_restart.o $(SRC)
	gcc -O3 -o EmptyRestart empty_restart.o $(SRC) -lrt -lTASLimited
	cp ../../controllers/Empty ./

clean:
	rm -f *.o
	rm -rf Empty
	rm -rf EmptyRestart
	rm -rf Empty_fd_server
