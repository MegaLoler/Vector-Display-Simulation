vector: vector.cpp
	g++ -Wall -Wpedantic -O3 -lglfw -o vector vector.cpp

.PHONY:
run: vector
	./vector

.PHONY:
clean:
	rm vector
