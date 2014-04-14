RESEARCH=/home/jcmarsh/research
PLAYERC=$(RESEARCH)/player
INC=-I$(PLAYERC)/ -I$(PLAYERC)/client_libs/ -I$(PLAYERC)/build/

all: regular

regular:
	gcc $(INC) -o art_pot art_pot.c -lplayerc -lrt
	gcc $(INC) -o redundant_test redundant_test.c -lplayerc

clean:
	rm -f art_pot
	rm -f redundant_test
