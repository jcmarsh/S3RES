RESEARCH=/home/jcmarsh/research
PLAYERC=$(RESEARCH)/player
INC=-I$(PLAYERC)/ -I$(PLAYERC)/client_libs/ -I$(PLAYERC)/build/

all: regular

regular:
	g++ $(INC) -o art_pot_p art_pot_p.c ../src/cpu.cpp ../src/time.cpp ../src/scheduler.cpp ../src/force.cpp ../src/taslimited.cpp -lplayerc -lrt
	gcc $(INC) -o redundant_test redundant_test.c -lplayerc

clean:
	rm -f art_pot_p
	rm -f redundant_test
