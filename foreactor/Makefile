CC=g++
CXXFLAGS=-O3 -Wall -Werror -std=c++20 -D_GNU_SOURCE

FOREACTOR_PATH=$(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))
FOREACTOR_SRCS=$(wildcard $(FOREACTOR_PATH)/*.cpp)
FOREACTOR_OBJS=$(patsubst %.cpp, %.o, $(FOREACTOR_SRCS))
FOREACTOR_INC=-I$(FOREACTOR_PATH)/include

LIBURING_INC=-I/home/jose/Repos/misc/liburing/src/include
LIBURING_LIB=-L/home/jose/Repos/misc/liburing/src -luring -Wl,-rpath=/home/jose/Repos/misc/liburing/src


.PHONY: all
all: foreactor


%.o: %.cpp include/%.hpp
	$(CC) -c -fPIC $(CXXFLAGS) $(FOREACTOR_INC) $(LIBURING_INC) $< -o $@ $(LIBURING_LIB)

foreactor: $(FOREACTOR_OBJS)
	$(CC) $^ -shared -o lib$@.so $(LIBURING_LIB)


.PHONY: clean
clean:
	rm -f *.o lib*.so
