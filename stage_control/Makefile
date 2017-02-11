include ../directory_paths

R_INC=-I$(PLAYERC)/ -I$(PLAYERC)/client_libs/ -I$(PLAYERC)/build/

regular:
	gcc $(R_INC) -o basic basic.c -lplayerc

clean:
	rm -f basic
