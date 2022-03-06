CC=g++
CXXFLAGS=-Wall -Werror -std=c++20 -D_GNU_SOURCE -O3 -DNDEBUG

FA_SRCS:=$(wildcard core/*.cpp)
FA_SRCS+=$(wildcard utils/*.cpp)
FA_OBJS:=$(patsubst %.cpp,%.o,$(FA_SRCS))
FA_INCL:=-I./include -I./utils


.PHONY: all
all: libforeactor.so


%.o: %.cpp
	$(CC) $(CXXFLAGS) -c -fPIC $(FA_INCL) $< -o $@ -luring -ldl

libforeactor.so: $(FA_OBJS)
	$(CC) $(CXXFLAGS) -fPIC -shared $^ -o $@ -luring -ldl


.PHONY: clean
clean:
	rm -f $(FA_OBJS) lib*.so
