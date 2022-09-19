CC=g++
CXXFLAGS=-Wall -Werror -std=c++20 -D_GNU_SOURCE
CXXLIBS=-luring -ldl -pthread

BPT_DIR:=bptree-src
BPT_INCL:=-I./$(BPT_DIR)
BPT_DLIB:=-L./$(BPT_DIR) -lbptree -Wl,-rpath=./$(BPT_DIR)

CLI_SRCS:=$(wildcard ./client-src/*.cpp)
CLI_OBJS:=$(patsubst %.cpp,%.o,$(CLI_SRCS))

FA_PATH:=../../libforeactor
FA_DLIB:=-L$(FA_PATH) -lforeactor -Wl,-rpath=$(FA_PATH)


.PHONY: all
all: release


.PHONY: release
release: CXXFLAGS+=-O3 -DNDEBUG
release: SUBMAKE_TYPE=release
release: bptree bptcli

.PHONY: debug
debug: CXXFLAGS+=-Og -g -ggdb
debug: SUBMAKE_TYPE=debug
debug: bptree bptcli


.PHONY: bptree
bptree:
	$(MAKE) -C $(BPT_DIR) $(SUBMAKE_TYPE)


$(CLI_OBJS): %.o: %.cpp
	$(CC) $(CXXFLAGS) -c $(BPT_INCL) $< -o $@ $(CXXLIBS) $(BPT_DLIB) $(FA_DLIB)

bptcli: bptree $(CLI_OBJS)
	$(CC) $(CXXFLAGS) $(CLI_OBJS) -o $@ $(CXXLIBS) $(BPT_DLIB) $(FA_DLIB)


.PHONY: clean
clean:
	$(MAKE) -C $(BPT_DIR) clean
	rm -f $(CLI_OBJS) bptcli
