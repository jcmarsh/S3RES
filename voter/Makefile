LIB=-lrt -lTASLimited
FLAGS=-O3 -static

all: VoterD

%.o: %.c
	gcc -c $(FLAGS) -o $@ $< 

VoterD: voterd.o ../src/commtypes.c ../src/replicas.c ../src/fd_server.c ../src/fd_client.c ../include/*.h
	gcc $(FLAGS) -o VoterD voterd.o ../src/commtypes.c ../src/replicas.c ../src/fd_server.c ../src/fd_client.c $(LIB)

copy:
	cp VoterD ../../stage/experiments/

clean:
	rm -f *.o
	rm -f VoterD
