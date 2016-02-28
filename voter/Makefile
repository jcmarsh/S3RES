include ../directory_paths

LIB=-lrt -lTASLimited
INC=-I../include/ -I../tas_lib/inc/
COMPILE=diet -Os gcc -O3 -nostdinc -Werror -static
#COMPILE=gcc -O3 -Werror -static

all: VoterD VoterM

%.o: %.c ../include/*.h ../tas_lib/inc/*.h
	$(COMPILE) -c -o $@ $< $(INC)

VoterD: voterd.o vote_buff.c ../src/commtypes.c ../src/replicas.c ../src/fd_server.c ../src/fd_client.c
	$(COMPILE) -o VoterD voterd.o vote_buff.c ../src/commtypes.c ../src/replicas.c ../src/fd_server.c ../src/fd_client.c $(INC) $(LIB)

VoterM: voterm.o fd_serverm.c voterm.h
	$(COMPILE) -o VoterM voterm.o fd_serverm.c $(INC) $(LIB)

test: test_buff.o vote_buff.c
	gcc -g -o testBuff test_buff.o vote_buff.c $(INC) $(LIB)

copy:
	cp VoterD $(INSTALL_DIR)
	cp VoterM $(INSTALL_DIR)

clean:
	rm -f *.o
	rm -f VoterD
	rm -f VoterM
	rm -f testBuff
