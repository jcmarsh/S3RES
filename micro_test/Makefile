SRC=../src/commtypes.c ../src/replicas.c ../src/fd_server.c

default:
	gcc -O3 -o EmptyBench empty_bench.c $(SRC) -lrt -lTASLimited
	gcc -O3 -o EmptyRestart empty_restart.c $(SRC) -lrt -lTASLimited

clean:
	rm -rf EmptyBench
	rm -rf EmptyRestart
	rm -rf Empty_fd_server
