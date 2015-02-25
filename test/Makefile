SRC=../src/commtypes.c ../src/replicas.c ../src/fd_server.c

default:
	gcc -o MapperTest mapper_test.c $(SRC) -lrt -lTASLimited
	gcc -o FilterTest filter_test.c $(SRC) -lrt -lTASLimited
	gcc -o ArtPotTest art_pot_test.c $(SRC) -lrt -lTASLimited
	gcc -o AStarTest a_star_test.c $(SRC) -lrt -lTASLimited

clean:
	rm -rf MapperTest
	rm -rf MapperTest_fd_server
	rm -rf FilterTest
	rm -rf FilterTest_fd_server
	rm -rf ArtPotTest
	rm -rf ArtPotTest_fd_server
	rm -rf AStarTest
	rm -rf AStarTest_fd_server
