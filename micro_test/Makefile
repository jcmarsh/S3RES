SRC=../src/commtypes.c ../src/replicas.c ../src/fd_server.c
INC=../include

all: EmptyBench EmptyRestart

%.o: %.c
	gcc -c -O3 -o $@ $<

EmptyBench: empty_bench.o $(SRC)
	gcc -O3 -o EmptyBench empty_bench.o $(SRC) -lrt -lTASLimited

EmptyRestart: empty_restart.o $(SRC)
	gcc -O3 -o EmptyRestart empty_restart.o $(SRC) -lrt -lTASLimited

clean:
	rm -f *.o
	rm -rf EmptyBench
	rm -rf EmptyRestart
	rm -rf Empty_fd_server
