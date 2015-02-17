SRC=../src/commtypes.cpp ../src/replicas.cpp ../src/fd_server.cpp

default:
	g++ -O3 -o EmptyBench empty_bench.c $(SRC) -lrt -lTASLimited
	g++ -O3 -o EmptyRestart empty_restart.c $(SRC) -lrt -lTASLimited

clean:
	rm -rf EmptyBench
	rm -rf EmptyRestart
	rm -rf Empty_fd_server
