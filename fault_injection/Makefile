include ../directory_paths

%.o: %.c
	gcc -c -o $@ $<

inject_error: register_util.o print_registers.h
	gcc -o inject_error register_util.o

copy:
	cp inject_error $(INSTALL_DIR)
	cp injector.py $(INSTALL_DIR)

clean:
	rm -f *.o
	rm -f inject_error


