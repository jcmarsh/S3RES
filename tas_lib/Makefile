default:
	gcc -fpic -c taslimited.c cpu.c force.c scheduler.c time.c
	gcc -shared -o libTASLimited.so taslimited.o cpu.o force.o scheduler.o time.o

install: default
	cp libTASLimited.so /usr/local/lib/

clean:
	rm -rf *.o
	rm -rf libTASLimited.so

