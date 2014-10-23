pb: pb.l pb.y
	bison -d pb.y
	flex pb.l
	cc -o plumber pb.tab.c lex.yy.c -lfl

clean:
	rm -rf plumber
	rm -rf lex.yy.c
	rm -rf pb.tab.c
	rm -rf pb.tab.h

