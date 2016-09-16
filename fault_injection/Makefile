include ../directory_paths

COMPILE=diet -Os gcc -O3 -nostdinc -Werror -static

all: c_injector

%.o: %.c
	gcc -c -o $@ $<

c_injector: c_injector.c
	$(COMPILE) -o c_injector c_injector.c -I../tas_lib/inc -lTASLimitedDiet

# bit_flipper only works on x86_64 right now.
bit_flipper: register_util.o print_registers.h
	gcc -o bit_flipper register_util.o

copy:
#	cp bit_flipper     $(INSTALL_DIR)
	cp c_injector      $(INSTALL_DIR)
	cp injector.py     $(INSTALL_DIR)
	cp player_to_rt.py $(INSTALL_DIR)

clean:
	rm -f *.o
	rm -f bit_flipper
	rm -f c_injector
