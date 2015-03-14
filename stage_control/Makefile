RESEARCH=/home/jcmarsh/research
PLAYERC=$(RESEARCH)/player
R_INC=-I$(PLAYERC)/ -I$(PLAYERC)/client_libs/ -I$(PLAYERC)/build/

regular:
	gcc $(R_INC) -o basic basic.c -lplayerc
	gcc $(R_INC) -o logger logger.c -lplayerc -lm

clean:
	rm -f basic
	rm -f logger
