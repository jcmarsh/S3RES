SRC=../src/commtypes.cpp ../src/replicas.cpp ../src/fd_server.cpp

default:
	g++ -o MapperTest mapper_test.c $(SRC)
	g++ -o FilterTest filter_test.c $(SRC)
	g++ -o ArtPotTest art_pot_test.c $(SRC)
	g++ -o AStarTest a_star_test.c $(SRC)

clean:
	rm -rf MapperTest
	rm -rf Mapper_fd_server
	rm -rf FilterTest
	rm -rf Filter_fd_server
	rm -rf ArtPotTest
	rm -rf ArtPot_fd_server
	rm -rf AStarTest
	rm -rf AStar_fd_server
