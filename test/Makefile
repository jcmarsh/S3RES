SRC=../src/commtypes.c ../src/replicas.c ../src/fd_server.c ../voter/vote_buff.c
INC=-I../include/

all: Controllers MapperTest FilterTest ArtPotTest AStarTest EmptyTest #LoadTest

%.o: %.c
	gcc -c -o $@ $< $(INC)

Micro:
	make -C ./micro_test

Controllers:
	make -C ../controllers

MapperTest: mapper_test.o $(SRC)
	gcc -o MapperTest mapper_test.o $(SRC) -lrt -lTASLimited
	cp ../controllers/Mapper ./

FilterTest: filter_test.o $(SRC)
	gcc -o FilterTest filter_test.o $(SRC) -lrt -lTASLimited
	cp ../controllers/Filter ./

ArtPotTest: art_pot_test.o $(SRC)
	gcc -o ArtPotTest art_pot_test.o $(SRC) -lrt -lTASLimited
	cp ../controllers/ArtPot ./

AStarTest: a_star_test.o $(SRC)
	gcc -o AStarTest a_star_test.o $(SRC) -lrt -lTASLimited
	cp ../controllers/AStar ./

EmptyTest: empty_test.o $(SRC)
	gcc -o EmptyTest empty_test.o $(SRC) -lrt -lTASLimited
	cp ../controllers/Empty ./

LoadTest: load_test.o $(SRC)
	gcc -o LoadTest load_test.o $(SRC) -lrt -lTASLimited
	cp ../controllers/Load ./

GenericEmptyTest: generic_empty_test.o $(SRC)
	gcc -o GenericEmptyTest generic_empty_test.o $(SRC) -lrt -lTASLimited

clean:
	make -C ./micro_test clean
	rm -f *.o
	rm -rf MapperTest
	rm -rf MapperTest_fd_server
	rm -rf Mapper
	rm -rf FilterTest
	rm -rf FilterTest_fd_server
	rm -rf Filter
	rm -rf ArtPotTest
	rm -rf ArtPotTest_fd_server
	rm -rf ArtPot
	rm -rf AStarTest
	rm -rf AStarTest_fd_server
	rm -rf AStar
	rm -rf EmptyTest
	rm -rf EmptyTest_fd_server
	rm -rf Empty
	rm -rf LoadTest
	rm -rf LoadTest_fd_server
	rm -rf Load
	rm -rf GenericEmptyTest
