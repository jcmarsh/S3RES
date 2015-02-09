all: regular

regular: 
	make -C ./controllers
	make -C ./plumber
	make -C ./fault_injection

install:
	make -C ./controllers copy
	make -C ./plumber copy
	make -C ./fault_injection copy

clean:
	make -C ./controllers clean
	make -C ./plumber clean
	make -C ./fault_injection clean

