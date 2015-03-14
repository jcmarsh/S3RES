SRC=../src/commtypes.c ../src/replicas.c ../src/fd_server.c

all: MapperTest FilterTest ArtPotTest AStarTest

%.o: %.c
	gcc -c -o $@ $<

MapperTest: mapper_test.o $(SRC)
	gcc -o MapperTest mapper_test.o $(SRC) -lrt -lTASLimited

FilterTest: filter_test.o $(SRC)
	gcc -o FilterTest filter_test.o $(SRC) -lrt -lTASLimited

ArtPotTest: art_pot_test.o $(SRC)
	gcc -o ArtPotTest art_pot_test.c $(SRC) -lrt -lTASLimited

AStarTest: a_star_test.o $(SRC)
	gcc -o AStarTest a_star_test.c $(SRC) -lrt -lTASLimited

clean:
	rm *.o
	rm -rf MapperTest
	rm -rf MapperTest_fd_server
	rm -rf FilterTest
	rm -rf FilterTest_fd_server
	rm -rf ArtPotTest
	rm -rf ArtPotTest_fd_server
	rm -rf AStarTest
	rm -rf AStarTest_fd_server
