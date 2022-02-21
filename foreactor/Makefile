CC=g++
CXXFLAGS=-O3 -DNDEBUG -Werror -Wno-unused-variable -std=c++20 -D_GNU_SOURCE

FOREACTOR_PATH=$(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))
FOREACTOR_SRCS=$(wildcard $(FOREACTOR_PATH)/*.cpp)
FOREACTOR_OBJS=$(patsubst %.cpp, %.o, $(FOREACTOR_SRCS))
FOREACTOR_INC=-I$(FOREACTOR_PATH)/include


.PHONY: all
all: foreactor


%.o: %.cpp
	$(CC) -c -fPIC $(CXXFLAGS) $(FOREACTOR_INC) $< -o $@ -luring -ldl

foreactor: $(FOREACTOR_OBJS)
	$(CC) $^ -shared -o lib$@.so -luring -ldl


.PHONY: clean
clean:
	rm -f *.o lib*.so
