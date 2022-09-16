CC=g++
CXXFLAGS=-O3 -std=c++20 -Wall -Werror
CXXLIBS=-pthread

SRCS:=$(wildcard ./*.cpp)
OBJS:=$(patsubst %.cpp,%.o,$(SRCS))


.PHONY: all
all: motivation


$(OBJS): %.o: %.cpp
	$(CC) $(CXXFLAGS) -c $< -o $@ $(CXXLIBS)

motivation: $(OBJS)
	$(CC) $(CXXFLAGS) $^ -o $@ $(CXXLIBS)


.PHONY: clean
clean:
	rm -f *.o motivation
