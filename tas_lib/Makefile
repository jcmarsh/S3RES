COMPILE=/usr/local/musl/bin/musl-gcc -fPIC -c -O3
#COMPILE=gcc -O3 -fPIC -c
#-g -O0 -fPIC -c

default: libTASLimited.a

%.o: %.c inc/*.h
	$(COMPILE) -o $@ $<
#	gcc $(FLAGS) -o $@ $<

libTASLimited.a: taslimited.o cpu.o force.o tas_time.o
	ar -cvq libTASLimited.a *.o

install: libTASLimited.a
	cp libTASLimited.a /usr/local/lib/

clean:
	rm -rf *.o
	rm -rf libTASLimited.a

