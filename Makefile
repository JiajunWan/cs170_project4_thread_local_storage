all: auto_grader

auto_grader: autograder_main.c tls_lib 
	g++ autograder_main.c tls.o -o autograder_main -pthread

tls_lib: tls.cpp tls.h
	g++ -c tls.cpp -o tls.o -pthread

clean:
	rm -f autograder_main *.o
