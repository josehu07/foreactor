CC=g++
CXXFLAGS=-Wall -Werror -std=c++20 -D_GNU_SOURCE
CXXLIBS=-luring -ldl -pthread

LDB_DIR:=leveldb-src
LDB_INCL:=-I./$(LDB_DIR)/include
LDB_DLIB:=-L./$(LDB_DIR)/build -lleveldb -Wl,-rpath=./$(LDB_DIR)/build

CLI_SRCS:=$(wildcard ./ycsbcli-src/*.cpp)
CLI_OBJS:=$(patsubst %.cpp,%.o,$(CLI_SRCS))

FA_DLIB:=-L../../libforeactor -lforeactor -Wl,-rpath=../../libforeactor


.PHONY: all
all: release


.PHONY: release
release: CXXFLAGS+=-O3 -DNDEBUG
release: CMAKE_TYPE=Release
release: leveldb ycsbcli

.PHONY: debug
debug: CXXFLAGS+=-Og -g -ggdb
debug: CMAKE_TYPE=Debug
debug: leveldb ycsbcli


.PHONY: leveldb
leveldb:
	mkdir -p $(LDB_DIR)/build
	cd $(LDB_DIR)/build && \
		cmake -DCMAKE_BUILD_TYPE=$(CMAKE_TYPE) -DCMAKE_CXX_STANDARD=20 .. && \
		make -j$(shell nproc)


$(CLI_OBJS): %.o: %.cpp
	$(CC) $(CXXFLAGS) -c $(LDB_INCL) $< -o $@ $(CXXLIBS) $(LDB_DLIB) $(FA_DLIB)

ycsbcli: leveldb $(CLI_OBJS)
	$(CC) $(CXXFLAGS) $(CLI_OBJS) -o $@ $(CXXLIBS) $(LDB_DLIB) $(FA_DLIB)


.PHONY: clean
clean:
	rm -rf $(LDB_DIR)/build
	rm -f $(CLI_OBJS) ycsbcli
