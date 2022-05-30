CC=g++
CXXFLAGS=-Wall -Werror -std=c++20 -O3 -DNDEBUG

LDB_DIR:=leveldb-src
LDB_INCL:=-I./$(LDB_DIR)/include
LDB_DLIB:=-L./$(LDB_DIR)/build -lleveldb -Wl,-rpath=./$(LDB_DIR)/build

CLI_SRCS:=$(wildcard ./ycsbcli-src/*.cpp)
CLI_OBJS:=$(patsubst %.cpp,%.o,$(CLI_SRCS))

FA_DLIB:=-L../foreactor -lforeactor -Wl,-rpath=../foreactor


.PHONY: all
all: leveldb ycsbcli


.PHONY: leveldb
leveldb:
	mkdir -p $(LDB_DIR)/build
	cd $(LDB_DIR)/build && \
		cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_STANDARD=20 .. && \
		make -j$(shell nproc)


$(CLI_OBJS): %.o: %.cpp
	$(CC) $(CXXFLAGS) -c $(LDB_INCL) $< -o $@ -lpthread $(LDB_DLIB) $(FA_DLIB)

ycsbcli: leveldb $(CLI_OBJS)
	$(CC) $(CXXFLAGS) $(CLI_OBJS) -o $@ -lpthread $(LDB_DLIB) $(FA_DLIB)


.PHONY: clean
clean:
	rm -rf $(LDB_DIR)/build
	rm -f $(CLI_OBJS) ycsbcli
