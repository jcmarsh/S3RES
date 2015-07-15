LIB=-lrt -lTASLimited
INC=-I../include/ -I../tas_lib/inc/
FLAGS=-O3 -Werror -static

all: VoterD

%.o: %.c ../include/*.h ../tas_lib/inc/*.h
	gcc -c $(FLAGS) -o $@ $< $(INC)

VoterD: voterd.o vote_buff.c ../src/commtypes.c ../src/replicas.c ../src/fd_server.c ../src/fd_client.c $(INC)
	gcc $(FLAGS) -o VoterD voterd.o vote_buff.c ../src/commtypes.c ../src/replicas.c ../src/fd_server.c ../src/fd_client.c $(INC) $(LIB)

test: test_buff.o vote_buff.c
	gcc -g -o testBuff test_buff.o vote_buff.c $(INC) $(LIB)

copy:
	cp VoterD ../../stage/experiments/

clean:
	rm -f *.o
	rm -f VoterD
	rm -f testBuff
