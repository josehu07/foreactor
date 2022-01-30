CC=g++
CXXFLAGS=-O3 -Wall -Werror -std=c++20 -D_GNU_SOURCE

LIBURING_INC=-I/home/jose/Repos/misc/liburing/src/include
LIBURING_LIB=-L/home/jose/Repos/misc/liburing/src -luring


.PHONY: all clean

all: makedb ldbget

makedb: makedb.cpp
	$(CC) $(CXXFLAGS) $^ -o $@

ldbget: ldbget.cpp
	$(CC) $(CXXFLAGS) $(LIBURING_INC) $^ -o $@ $(LIBURING_LIB)

clean:
	rm -f makedb ldbget
