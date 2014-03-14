SRC=./src
INC=-I./include
BUILD=./build
CC=gcc
CFLAGS=-Wall -g

default: clean select

select:
	$(CC) $(FLAGS) -o $(BUILD)/select $(SRC)/select.c $(INC)

#test:
#	gcc -o test -g test.c utility.h

clean:
	rm -f $(BUILD)/*

