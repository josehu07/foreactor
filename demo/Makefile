CC=g++
CXXFLAGS=-O3 -Wall -Werror -std=c++20 -D_GNU_SOURCE

FOREACTOR_INC=-I../foreactor/include
FOREACTOR_LIB=-L../foreactor -lforeactor -Wl,-rpath=../foreactor

LIBURING_INC=-I/home/jose/Repos/misc/liburing/src/include
LIBURING_LIB=-L/home/jose/Repos/misc/liburing/src -luring -Wl,-rpath=/home/jose/Repos/misc/liburing/src


.PHONY: all clean

all: makedb ldbget

makedb: makedb.cpp
	$(CC) $(CXXFLAGS) $^ -o $@

ldbget: ldbget.cpp
	$(CC) $(CXXFLAGS) $(FOREACTOR_INC) $(LIBURING_INC)  $^ -o $@ $(FOREACTOR_LIB) $(LIBURING_LIB)

clean:
	rm -f makedb ldbget
