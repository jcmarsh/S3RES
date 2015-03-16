RESEARCH=/home/jcmarsh/research
PLAYERC=$(RESEARCH)/player
R_INC=-I$(PLAYERC)/ -I$(PLAYERC)/client_libs/ -I$(PLAYERC)/build/

regular:
	gcc $(R_INC) -o basic basic.c -lplayerc

clean:
	rm -f basic
