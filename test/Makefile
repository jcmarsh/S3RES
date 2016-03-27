SRC=../src/commtypes.c ../src/replicas.c ../src/fd_server.c ../voter/vote_buff.c
INC=-I../include/ -I../tas_lib/inc/
LIB=-lpthread -lrt -lTASLimited

all: vote

vote: Controllers Voters AStarVoteTest MapperVoteTest FilterVoteTest EmptyVoteTest ArtPotVoteTest GenericEmptyVoteTest

%.o: %.c
	gcc -c -o $@ $< $(INC)

Controllers:
	make -C ../controllers

Voters:
	cp ../fault_injection/injector.py ./
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
	gcc -o GenericEmptyVoteTest generic_empty_vote_test.o $(SRC) $(LIB)
	cp ../controllers/GenericEmpty ./

EmptyVoteTest: empty_vote_test.o $(SRC)
	gcc -o EmptyVoteTest empty_vote_test.o $(SRC) $(LIB)
	cp ../controllers/Empty ./

AStarVoteTest: a_star_vote_test.o $(SRC)
	gcc -o AStarVoteTest a_star_vote_test.o $(SRC) $(LIB)
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
	rm -f Filter_fd_server
	rm -f Filter
	rm -f ArtPotVoteTest
	rm -f ArtPot_fd_server
	rm -f ArtPot
	rm -f AStarVoteTest
	rm -f AStar_fd_server
	rm -f AStar
	rm -f EmptyVoteTest
	rm -f Empty_fd_server
	rm -f Empty
	rm -f LoadTest
	rm -f Load
	rm -f GenericEmptyVoteTest
	rm -f GenericEmpty
	rm -f GenericEmpty_fd_server
	rm -f VoterD
	rm -f VoterM
