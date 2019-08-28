vector: vector.cpp
	g++ -Wall -Wpedantic -O3 -Iinclude -lglfw -ldl -o vector vector.cpp glad.c

.PHONY:
run: vector
	./vector

.PHONY:
clean:
	rm vector
