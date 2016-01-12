FLAGS=-O3 -fPIC -c
#FLAGS=-g -O0 -fPIC -c

%.o: %.c inc/*.h
	gcc $(FLAGS) -o $@ $<

libTASLimited.a: taslimited.o cpu.o force.o tas_time.o
	ar -cvq libTASLimited.a *.o

install: libTASLimited.a
	cp libTASLimited.a /usr/local/lib/

clean:
	rm -rf *.o
	rm -rf libTASLimited.a

