CC=g++
CXXFLAGS=-Wall -Werror -std=c++20 -O3 -DNDEBUG

SRCS:=$(wildcard ./*.cpp)
OBJS:=$(patsubst %.cpp,%.o,$(SRCS))


.PHONY: all
all: motivation


$(OBJS): %.o: %.cpp
	$(CC) $(CXXFLAGS) -c $< -o $@

motivation: $(OBJS)
	$(CC) $(CXXFLAGS) $^ -o $@ -lpthread -luring


.PHONY: clean
clean:
	rm -f $(OBJS) motivation
