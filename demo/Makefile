CC=g++
CXXFLAGS=-O3 -DNDEBUG -Wall -Werror -Wno-unused-variable -std=c++20 -D_GNU_SOURCE

FOREACTOR_INC=-I../foreactor/include
FOREACTOR_LIB=-L../foreactor -lforeactor -Wl,-rpath=../foreactor


.PHONY: all clean

all: makedb ldbget

makedb: makedb.cpp
	$(CC) $(CXXFLAGS) $^ -o $@

ldbget: ldbget.cpp
	$(CC) $(CXXFLAGS) $(FOREACTOR_INC) $^ -o $@ $(FOREACTOR_LIB)

clean:
	rm -f makedb ldbget
