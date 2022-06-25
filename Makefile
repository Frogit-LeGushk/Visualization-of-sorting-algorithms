CC=g++
SOURCES=main.cpp tests.cpp

build_and_run_main: build run_main
build_and_run_tests: build run_tests

build: $(SOURCES)
	$(CC) -E -D ASSEMBLY_LIB=1 main.cpp -o main_prep.cpp
	$(CC) -o libSorts.so -shared -fPIC main_prep.cpp
	-rm main_prep.cpp 2>/dev/null
	$(CC) main.cpp -lc -lpthread -lncurses -o main
	#export LD_LIBRARY_PATH=.
	$(CC) tests.cpp -L. -lSorts -lc -lpthread -lncurses -o tests

run_main: main
	./main

run_tests: tests
	./tests

clean:
	-rm main 2>/dev/null
	-rm tests 2>/dev/null
	-rm a.out 2>/dev/null
	-rm libSorts.so 2>/dev/null
	-rm *.txt 2>/dev/null



