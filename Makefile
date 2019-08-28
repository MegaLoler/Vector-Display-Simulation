vector: vector.cpp
	g++ -Wall -Wpedantic -O3 -Iinclude -lglfw -ldl -o vector vector.cpp glad.c

.PHONY:
run: vector
	MESA_GL_VERSION_OVERRIDE=3.3 ./vector

.PHONY:
clean:
	rm vector
