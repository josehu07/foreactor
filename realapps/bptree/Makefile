CC=g++
CXXFLAGS=-Wall -Werror -std=c++20 -D_GNU_SOURCE
CXXLIBS=-luring -ldl

BPT_SRCS:=$(wildcard ./bptree-src/*.cpp)
BPT_HDRS:=$(wildcard ./bptree-src/*.hpp)
BPT_OBJS:=$(patsubst %.cpp,%.o,$(BPT_SRCS))
BPT_INCL:=-I./bptree-src

PLG_SRCS:=$(wildcard ./bptree-plg/*.cpp)
PLG_HDRS:=$(wildcard ./bptree-plg/*.hpp)
PLG_OBJS:=$(patsubst %.cpp,%.o,$(PLG_SRCS))
PLG_INCL:=-I./bptree-plg

CLI_SRCS:=$(wildcard ./client-src/*.cpp)
CLI_OBJS:=$(patsubst %.cpp,%.o,$(CLI_SRCS))

FA_PATH:=../../libforeactor
FA_INCL:=-I$(FA_PATH)/include
FA_DLIB:=-L$(FA_PATH) -lforeactor -Wl,-rpath=$(FA_PATH)


.PHONY: all
all: release


.PHONY: release
release: CXXFLAGS+=-O3 -DNDEBUG
release: SUBMAKE_TYPE=release
release: bptcli

.PHONY: debug
debug: CXXFLAGS+=-Og -g -ggdb
debug: SUBMAKE_TYPE=debug
debug: bptcli


$(BPT_OBJS): %.o: %.cpp $(BPT_HDRS)
	$(CC) $(CXXFLAGS) -c $(BPT_INCL) $(PLG_INCL) $(FA_INCL) $< -o $@ $(CXXLIBS) $(FA_DLIB)

$(PLG_OBJS): %.o: %.cpp $(PLG_HDRS)
	$(CC) $(CXXFLAGS) -c $(BPT_INCL) $(PLG_INCL) $(FA_INCL) $< -o $@ $(CXXLIBS) $(FA_DLIB)

$(CLI_OBJS): %.o: %.cpp
	$(CC) $(CXXFLAGS) -c $(BPT_INCL) $(PLG_INCL) $(FA_INCL) $< -o $@ $(CXXLIBS) $(FA_DLIB)

bptcli: $(CLI_OBJS) $(BPT_OBJS) $(PLG_OBJS)
	$(CC) $(CXXFLAGS) $(CLI_OBJS) $(BPT_OBJS) $(PLG_OBJS) -o $@ $(CXXLIBS) $(FA_DLIB)


.PHONY: clean
clean:
	rm -f $(CLI_OBJS) $(BPT_OBJS) $(PLG_OBJS) bptcli
