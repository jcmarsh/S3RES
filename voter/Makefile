include ../directory_paths

LIB=-lrt -lTASLimited
INC=-I../include/ -I../tas_lib/inc/
FLAGS=-O3 -Werror -static

all: VoterD VoterR VoterM

%.o: %.c ../include/*.h ../tas_lib/inc/*.h
	gcc -c $(FLAGS) -o $@ $< $(INC)

VoterD: voterd.o vote_buff.c ../src/commtypes.c ../src/replicas.c ../src/fd_server.c ../src/fd_client.c
	gcc $(FLAGS) -o VoterD voterd.o vote_buff.c ../src/commtypes.c ../src/replicas.c ../src/fd_server.c ../src/fd_client.c $(INC) $(LIB)

VoterR: voterr.o fd_serverr.c
	gcc $(FLAGS) -o VoterR voterr.o fd_serverr.c $(INC) $(LIB)

VoterM: voterm.o fd_serverr.c
	gcc $(FLAGS) -o VoterM voterm.o fd_serverr.c $(INC) $(LIB)

test: test_buff.o vote_buff.c
	gcc -g -o testBuff test_buff.o vote_buff.c $(INC) $(LIB)

copy:
	cp VoterD $(INSTALL_DIR)
	cp VoterR $(INSTALL_DIR)
	cp VoterM $(INSTALL_DIR)

clean:
	rm -f *.o
	rm -f VoterD
	rm -f VoterR
	rm -f VoterM
	rm -f testBuff
