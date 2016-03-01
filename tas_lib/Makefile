#COMPILE=diet gcc -nostdinc -O3 -fPIC -c
COMPILE=/usr/local/musl/bin/musl-gcc -fPIC -c -O3
#COMPILE=gcc -O3 -fPIC -c
FLAGS= -O3 -fPIC -c
#-g -O0 -fPIC -c

default: libTASLimited.a libTASLimitedDiet.a

%.o: %.c inc/*.h
	$(COMPILE) -o $@ $<
#	gcc $(FLAGS) -o $@ $<

libTASLimited.a: taslimited.o cpu.o force.o tas_time.o
	ar -cvq libTASLimited.a *.o

libTASLimitedDiet.a: taslimited.c cpu.c force.c tas_time.c
	diet gcc -nostdinc $(FLAGS) -D NO_PTHREAD -o taslimitedd.o taslimited.c
	diet gcc -nostdinc $(FLAGS) -o cpud.o cpu.c
	diet gcc -nostdinc $(FLAGS) -o forced.o force.c
	diet gcc -nostdinc $(FLAGS) -o tas_timed.o tas_time.c
	ar -cvq libTASLimitedDiet.a taslimitedd.o cpud.o forced.o tas_timed.o

install: libTASLimited.a
	cp libTASLimited.a /usr/local/lib/
	cp libTASLimited.a /usr/local/musl/lib/
	cp libTASLimitedDiet.a /usr/local/lib/

clean:
	rm -rf *.o
	rm -rf libTASLimited.a
	rm -rf libTASLimitedDiet.a
