RESEARCH=/home/jcmarsh/research
PLAYERC=$(RESEARCH)/player
INC=-I$(PLAYERC)/ -I$(PLAYERC)/client_libs/ -I$(PLAYERC)/build/

all: regular

regular:
	g++ $(INC) -o art_pot_p art_pot_p.c ../src/cpu.cpp ../src/time.cpp ../src/scheduler.cpp ../src/force.cpp ../src/taslimited.cpp ../src/fd_client.cpp -lrt
	g++ $(INC) -o VoterB voterb.c ../src/cpu.cpp ../src/time.cpp ../src/scheduler.cpp ../src/force.cpp ../src/taslimited.cpp ../src/replicas.cpp ../src/fd_server.cpp -lrt
	g++ $(INC) -o BenchMarker benchmarker.c ../src/cpu.cpp ../src/time.cpp ../src/scheduler.cpp ../src/force.cpp ../src/taslimited.cpp ../src/replicas.cpp -lrt

copy: regular
	cp art_pot_p ../../stage/experiments/
	cp VoterB ../../stage/experiments/
	cp BenchMarker ../../stage/experiments/

clean:
	rm -f art_pot_p
	rm -f VoterB
	rm -f BenchMarker
