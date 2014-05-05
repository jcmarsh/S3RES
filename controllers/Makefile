RESEARCH=/home/jcmarsh/research
PLAYERC=$(RESEARCH)/player
INC=-I$(PLAYERC)/ -I$(PLAYERC)/client_libs/ -I$(PLAYERC)/build/

all: regular

regular:
	g++ $(INC) -O3 -o ArtPot art_pot_p.c ../src/cpu.cpp ../src/time.cpp ../src/scheduler.cpp ../src/force.cpp ../src/taslimited.cpp ../src/fd_client.cpp -lrt
	g++ $(INC) -O3 -o VoterB voterb.c ../src/cpu.cpp ../src/time.cpp ../src/scheduler.cpp ../src/force.cpp ../src/taslimited.cpp ../src/replicas.cpp ../src/fd_server.cpp -lrt
	g++ $(INC) -O3 -o BenchMarker benchmarker.c ../src/cpu.cpp ../src/time.cpp ../src/scheduler.cpp ../src/force.cpp ../src/taslimited.cpp ../src/replicas.cpp -lrt

copy: regular
	cp ArtPot ../../stage/experiments/
	cp VoterB ../../stage/experiments/
	cp BenchMarker ../../stage/experiments/

clean:
	rm -f ArtPot
	rm -f VoterB
	rm -f BenchMarker
