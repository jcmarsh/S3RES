default:
	g++ -fpic -c taslimited.cpp cpu.cpp force.cpp scheduler.cpp time.cpp
	g++ -shared -o libTASLimited.so taslimited.o cpu.o force.o scheduler.o time.o

install: default
	cp libTASLimited.so /usr/local/lib/

clean:
	rm -rf *.o
	rm -rf libTASLimited.so

