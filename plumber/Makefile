default: pb.l pb.y
	bison -d pb.y
	flex pb.l
	cp ../src/commtypes.cpp ./commtypes.c
	cc -g -o plumber commtypes.c plumbing.c pb.tab.c lex.yy.c -lfl

copy: default
	cp plumber ../../stage/experiments/

# It makes me cry that I am doing this hack.
# I can only get commtypes to build with player_translator if it is a .cpp
# and this is using .c files
clean:
	rm -rf commtypes.c
	rm -rf plumber
	rm -rf lex.yy.c
	rm -rf pb.tab.c
	rm -rf pb.tab.h

