LIB=-lrt -lTASLimited
INC=-I../include/ -I../tas_lib/inc/
FLAGS=-O3 -Werror -static

all: VoterD

%.o: %.c ../include/*.h ../tas_lib/inc/*.h
	gcc -c $(FLAGS) -o $@ $< $(INC)

VoterD: voterd.o vote_buff.c ../src/commtypes.c ../src/replicas.c ../src/fd_server.c ../src/fd_client.c 
	gcc $(FLAGS) -o VoterD voterd.o vote_buff.c ../src/commtypes.c ../src/replicas.c ../src/fd_server.c ../src/fd_client.c $(INC) $(LIB)

copy:
	cp VoterD ../../stage/experiments/

clean:
	rm -f *.o
	rm -f VoterD
