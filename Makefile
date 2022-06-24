CC=g++
SOURCES=main.cpp tests.cpp

build_and_run: build run_main

build: $(SOURCES)
	$(CC) -E -D ASSEMBLY_LIB=1 main.cpp -o main_prep.cpp
	$(CC) -o libSorts.so -shared -fPIC main_prep.cpp
	-rm main_prep.cpp 2>/dev/null
	$(CC) main.cpp -lc -lpthread -lncurses -o main
	$(CC) tests.cpp -L. -lSorts -lc -lpthread -lncurses -o tests

run_main: main
	./main

run_test: tests
	./tests

clean:
	-rm main 2>/dev/null
	-rm tests 2>/dev/null
	-rm a.out 2>/dev/null
	-rm libSorts.so 2>/dev/null
	-rm results.txt 2>/dev/null





