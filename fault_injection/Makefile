%.o: %.c
	gcc -c -o $@ $<

main: main.o register_util.o
	gcc -o inject_error main.o register_util.o

copy:
	cp inject_error ../../stage/experiments
	cp injector.py ../../stage/experiments

clean:
	rm *.o
	rm -f inject_error


