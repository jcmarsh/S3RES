SRC=../src/commtypes.c ../src/replicas.c ../src/fd_server.c ../voter/vote_buff.c
INC=-I../include/

all: comp vote

comp: Controllers MapperTest ArtPotTest AStarTest #LoadTest

vote: Controllers Voters FilterVoteTest ArtPotVoteTest GenericEmptyVoteTest

%.o: %.c
	gcc -c -o $@ $< $(INC)

Micro:
	make -C ./micro_test

Controllers:
	make -C ../controllers

Voters:
	make -C ../voter
	cp ../voter/VoterM ./
	cp ../voter/VoterD ./

MapperTest: mapper_test.o $(SRC)
	gcc -o MapperTest mapper_test.o $(SRC) -lrt -lTASLimited
	cp ../controllers/Mapper ./

FilterVoteTest: filter_vote_test.o $(SRC)
	gcc -o FilterVoteTest filter_vote_test.o $(SRC) -lrt -lTASLimited
	cp ../controllers/Filter ./

ArtPotTest: art_pot_test.o $(SRC)
	gcc -o ArtPotTest art_pot_test.o $(SRC) -lrt -lTASLimited
	cp ../controllers/ArtPot ./

ArtPotVoteTest: art_pot_vote_test.o $(SRC)
	gcc -o ArtPotVoteTest art_pot_vote_test.o $(SRC) -lrt -lTASLimited
	cp ../controllers/ArtPot ./

GenericEmptyVoteTest: generic_empty_vote_test.o $(SRC)
	gcc -o GenericEmptyVoteTest generic_empty_vote_test.o $(SRC) -lrt -lTASLimited
	cp ../controllers/GenericEmpty ./

AStarTest: a_star_test.o $(SRC)
	gcc -o AStarTest a_star_test.o $(SRC) -lrt -lTASLimited
	cp ../controllers/AStar ./

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
	rm -rf FilterVoteTest
	rm -rf FilterTest_fd_server
	rm -rf Filter_fd_server
	rm -rf Filter
	rm -rf ArtPotTest
	rm -rf ArtPotVoteTest
	rm -rf ArtPotTest_fd_server
	rm -rf ArtPot_fd_server
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
	rm -rf GenericEmptyVoteTest
	rm -rf GenericEmpty
	rm -rf VoterD
	rm -rf VoterM
