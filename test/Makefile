SRC=../src/commtypes.c ../src/replicas.c ../src/fd_server.c ../voter/vote_buff.c
INC=-I../include/ -I../tas_lib/inc/
LIB=-lpthread -lrt -lTASLimited

all: comp vote

comp: Controllers AStarTest #LoadTest

vote: Controllers Voters MapperVoteTest FilterVoteTest ArtPotVoteTest GenericEmptyVoteTest

%.o: %.c
	gcc -c -o $@ $< $(INC)

Controllers:
	make -C ../controllers

Voters:
	make -C ../voter
	cp ../voter/VoterM ./
	cp ../voter/VoterD ./

MapperVoteTest: mapper_vote_test.o $(SRC)
	gcc -o MapperVoteTest mapper_vote_test.o $(SRC) $(LIB)
	cp ../controllers/Mapper ./

FilterVoteTest: filter_vote_test.o $(SRC)
	gcc -o FilterVoteTest filter_vote_test.o $(SRC) $(LIB)
	cp ../controllers/Filter ./

ArtPotVoteTest: art_pot_vote_test.o $(SRC)
	gcc -o ArtPotVoteTest art_pot_vote_test.o $(SRC) $(LIB)
	cp ../controllers/ArtPot ./

GenericEmptyVoteTest: generic_empty_vote_test.o $(SRC)
	gcc -o GEVoteTest generic_empty_vote_test.o $(SRC) $(LIB)
	cp ../controllers/GenericEmpty ./
	cp ../fault_injection/injector.py ./

AStarTest: a_star_test.o $(SRC)
	gcc -o AStarTest a_star_test.o $(SRC) -lrt -lTASLimited
	cp ../controllers/AStar ./

LoadTest: load_test.o $(SRC)
	gcc -o LoadTest load_test.o $(SRC) -lrt -lTASLimited
	cp ../controllers/Load ./

clean:
	rm -f injector.py
	rm -f *.o
	rm -f MapperVoteTest
	rm -f Mapper_fd_server
	rm -f Mapper
	rm -f FilterVoteTest
	rm -f FilterTest_fd_server
	rm -f Filter_fd_server
	rm -f Filter
	rm -f ArtPotVoteTest
	rm -f ArtPotTest_fd_server
	rm -f ArtPot_fd_server
	rm -f ArtPot
	rm -f AStarTest
	rm -f AStarTest_fd_server
	rm -f AStar
	rm -f EmptyTest
	rm -f EmptyTest_fd_server
	rm -f Empty
	rm -f LoadTest
	rm -f LoadTest_fd_server
	rm -f Load
	rm -f GEVoteTest
	rm -f GenericEmpty
	rm -f GenericEmpty_fd_server
	rm -f VoterD
	rm -f VoterM
