CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -pedantic

snake: main.cpp
	$(CXX) $(CXXFLAGS) -o snake main.cpp

run: snake
	./snake

clean:
	rm -f snake snake.exe

.PHONY: run clean
