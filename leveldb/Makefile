CC=g++
CXXFLAGS=-Wall -Werror -O3

YCSBCLI_SRC=ycsbcli.cpp cxxopts.hpp

LEVLEDB_SRC=leveldb-1.23
LEVELDB_INC=-I./$(LEVLEDB_SRC)/include
LEVELDB_LIB=./$(LEVLEDB_SRC)/build/libleveldb.a


.PHONY: all
all: leveldb ycsbcli


.PHONY: leveldb
leveldb:
	mkdir -p $(LEVLEDB_SRC)/build
	cd $(LEVLEDB_SRC)/build && \
		cmake -DCMAKE_BUILD_TYPE=Release .. && \
		make -j$(shell nproc)

ycsbcli: leveldb $(YCSBCLI_SRC)
	$(CC) $(CXXFLAGS) $(LEVELDB_INC) -o ycsbcli $(YCSBCLI_SRC) $(LEVELDB_LIB) -lpthread


.PHONY: clean
clean:
	rm -rf $(LEVLEDB_SRC)/build
	rm -f ycsbcli
