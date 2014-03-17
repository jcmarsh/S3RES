SRC=./src
INC=-I./include
BUILD=./build
LIB=./lib
CC=gcc
CFLAGS=-Wall -g

default: test

test: pint
	$(CC) $(FLAGS) -o $(BUILD)/test $(SRC)/test.c $(INC) -L$(LIB)/ -lpint

pint: utility.o
	$(CC) -shared -o $(LIB)/libpint.so $(BUILD)/register_util.o $(BUILD)/replicas.o $(BUILD)/utility.o

utility.o: register_util.o replicas.o
	$(CC) -fPIC -o $(BUILD)/utility.o -c $(SRC)/utility.c $(INC)

register_util.o:
	$(CC) -fPIC -o $(BUILD)/register_util.o -c $(SRC)/register_util.c $(INC)

replicas.o:
	$(CC) -fPIC -o $(BUILD)/replicas.o -c $(SRC)/replicas.c $(INC)

clean:
	rm -f $(BUILD)/*
	rm -f $(LIB)/*

