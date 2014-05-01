RESEARCH=/home/jcmarsh/research
PLAYERC=$(RESEARCH)/player
INC=-I$(PLAYERC)/ -I$(PLAYERC)/client_libs/ -I$(PLAYERC)/build/

all: regular

regular:
	g++ $(INC) -o art_pot_p art_pot_p.c ../src/cpu.cpp ../src/time.cpp ../src/scheduler.cpp ../src/force.cpp ../src/taslimited.cpp -lrt
	g++ $(INC) -o VoterB voterb.c ../src/cpu.cpp ../src/time.cpp ../src/scheduler.cpp ../src/force.cpp ../src/taslimited.cpp ../src/replicas.cpp -lrt
	gcc $(INC) -o redundant_test redundant_test.c -lplayerc

install: regular
	cp art_pot_p ../../stage/experiments
	cp VoterB ../../stage/experiments

clean:
	rm -f art_pot_p
	rm -f VoterB
	rm -f redundant_test
