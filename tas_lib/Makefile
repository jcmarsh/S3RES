
%.o: %.c ../include/*.h
	gcc -O3 -fPIC -c -o $@ $<

libTASLimited.a: taslimited.o cpu.o force.o time.o
	ar -cvq libTASLimited.a *.o

install: libTASLimited.a
	cp libTASLimited.a /usr/local/lib/

clean:
	rm -rf *.o
	rm -rf libTASLimited.a

