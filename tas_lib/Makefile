FLAGS=-O3 -fPIC -c
#FLAGS=-g -O0 -fPIC -c

default: libTASLimited.a libTASLimitedDiet.a

%.o: %.c inc/*.h
	gcc $(FLAGS) -o $@ $<

libTASLimited.a: taslimited.o cpu.o force.o tas_time.o
	ar -cvq libTASLimited.a *.o

libTASLimitedDiet.a: taslimited.c cpu.c force.c tas_time.c
	diet gcc -nostdinc $(FLAGS) -o taslimitedd.o taslimited.c
	diet gcc -nostdinc $(FLAGS) -o cpud.o cpu.c
	diet gcc -nostdinc $(FLAGS) -o forced.o force.c
	diet gcc -nostdinc $(FLAGS) -o tas_timed.o tas_time.c
	ar -cvq libTASLimitedDiet.a taslimitedd.o cpud.o forced.o tas_timed.o

install: libTASLimited.a libTASLimitedDiet.a
	cp libTASLimited.a /usr/local/lib/
	cp libTASLimitedDiet.a /usr/local/lib/

clean:
	rm -rf *.o
	rm -rf libTASLimited.a
	rm -rf libTASLimitedDiet.a

