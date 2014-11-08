default: pb.l pb.y
	bison -d pb.y
	flex pb.l
	cc -g -o plumber ../src/commtypes.c plumbing.c pb.tab.c lex.yy.c -lfl

copy: default
	cp plumber ../../stage/experiments/

clean:
	rm -rf plumber
	rm -rf lex.yy.c
	rm -rf pb.tab.c
	rm -rf pb.tab.h

