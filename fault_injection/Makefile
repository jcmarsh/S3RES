%.o: %.c
	gcc -c -o $@ $<

inject_error: register_util.o print_registers.h
	gcc -o inject_error register_util.o

copy:
	cp inject_error ../../stage/experiments
	cp injector.py ../../stage/experiments

clean:
	rm -f *.o
	rm -f inject_error


