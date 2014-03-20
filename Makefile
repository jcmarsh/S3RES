SRC=./src
EXAMPLE=./examples
INC=-I./include
BUILD=./build
LIB=./lib
CC=gcc
CFLAGS=-Wall -g

default: examples tracee

tracee:
	$(CC) $(FLAGS) -o $(BUILD)/fib ./tracee_progs/fib.c

examples: pint
	$(CC) $(FLAGS) -o $(BUILD)/test $(EXAMPLE)/test.c $(INC) -lpint
	$(CC) $(FLAGS) -o $(BUILD)/hands $(EXAMPLE)/hands_off.c $(INC) -lpint

install: pint
	cp $(LIB)/libpint.so /usr/local/lib/libpint.so

uninstall:
	rm -f /usr/local/lib/libpint.so

pint: utility.o
	$(CC) -shared -g -o $(LIB)/libpint.so $(BUILD)/register_util.o $(BUILD)/replicas.o $(BUILD)/utility.o

utility.o: register_util.o replicas.o
	$(CC) -fPIC -g -o $(BUILD)/utility.o -c $(SRC)/utility.c $(INC)

register_util.o:
	$(CC) -fPIC -g -o $(BUILD)/register_util.o -c $(SRC)/register_util.c $(INC)

replicas.o:
	$(CC) -fPIC -g -o $(BUILD)/replicas.o -c $(SRC)/replicas.c $(INC)

clean:
	rm -f $(BUILD)/*
	rm -f $(LIB)/*

