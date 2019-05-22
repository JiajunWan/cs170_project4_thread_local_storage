all: auto_grader test

auto_grader: autograder_main.c tls_lib 
	g++ autograder_main.c tls.o -o autograder_main -pthread

test: test.c tls_lib
	g++ test.c tls.o -o test -pthread

tls_lib: tls.cpp tls.h
	g++ -c tls.cpp -o tls.o -pthread

clean:
	rm -f auto_grader test *.o
