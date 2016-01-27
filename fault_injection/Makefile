include ../directory_paths

all: c_injector

%.o: %.c
	gcc -c -o $@ $<

c_injector: c_injector.c
	gcc -Werror -o c_injector c_injector.c

# bit_flipper only works on x86_64 right now.
bit_flipper: register_util.o print_registers.h
	gcc -o bit_flipper register_util.o

copy:
#	cp bit_flipper $(INSTALL_DIR)
	cp c_injector  $(INSTALL_DIR)
	cp injector.py $(INSTALL_DIR)

clean:
	rm -f *.o
	rm -f bit_flipper
	rm -f c_injector
