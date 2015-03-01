RESEARCH=/home/jcmarsh/research
SPLAYERC=$(RESEARCH)/static_libplayerc
PLAYERC=$(RESEARCH)/player
LIBCDIR=$(RESEARCH)/dietlibc-0.33/bin-i386
INC=-I$(SPLAYERC)/ -I$(SPLAYERC)/player_build/ -I$(SPLAYERC)/player_include/
R_INC=-I$(PLAYERC)/ -I$(PLAYERC)/client_libs/ -I$(PLAYERC)/build/

all: regular

diet:
	diet gcc -nostdinc $(INC) -o basic basic.c $(SPLAYERC)/libplayerc.a $(LIBCDIR)/dietlibc.a $(LIBCDIR)/librpc.a $(LIBCDIR)/libm.a

regular:
	gcc $(R_INC) -o basic basic.c -lplayerc
	gcc $(R_INC) -o logger logger.c -lplayerc -lm

clean:
	rm -f basic
	rm -f logger
