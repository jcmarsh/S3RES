
%.o: %.c
	gcc -fpic -c -o $@ $<

libTASLimited.so: taslimited.o cpu.o force.o time.o
	gcc -shared -o libTASLimited.so taslimited.o cpu.o force.o time.o


install: libTASLimited.so
	cp libTASLimited.so /usr/local/lib/

clean:
	rm -rf *.o
	rm -rf libTASLimited.so

