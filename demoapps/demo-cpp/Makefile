CC=g++
CXXFLAGS=-Wall -Werror -std=c++20
CXXLIBS=-luring -pthread

APP_SRCS:=$(wildcard ./demo-cpp-src/*.cpp)
APP_OBJS:=$(patsubst %.cpp,%.o,$(APP_SRCS))
APP_INCL:=-I./demo-cpp-src

PLG_SRCS:=$(wildcard ./demo-cpp-plg/*.cpp)
PLG_OBJS:=$(patsubst %.cpp,%.o,$(PLG_SRCS))

FA_PATH:=../../libforeactor
FA_INCL:=-I$(FA_PATH)/include
FA_DLIB:=-L$(FA_PATH) -lforeactor -Wl,-rpath=$(FA_PATH)

WRAPS:=-Wl,--wrap=_Z12exper_simplePv -Wl,--wrap=_Z15exper_branchingPv -Wl,--wrap=_Z13exper_loopingPv
WRAPS+=-Wl,--wrap=_Z15exper_weak_edgePv -Wl,--wrap=_Z14exper_crossingPv
WRAPS+=-Wl,--wrap=_Z14exper_read_seqPv -Wl,--wrap=_Z15exper_write_seqPv
WRAPS+=-Wl,--wrap=_Z15exper_streamingPv


.PHONY: all
all: release


.PHONY: release
release: CXXFLAGS+=-O3 -DNDEBUG
release: demo

.PHONY: debug
debug: CXXFLAGS+=-Og -g -ggdb
debug: demo


$(APP_OBJS): %.o: %.cpp
	$(CC) $(CXXFLAGS) -c $< -o $@ $(CXXLIBS)

$(PLG_OBJS): %.o: %.cpp
	$(CC) $(CXXFLAGS) -c $(APP_INCL) $(FA_INCL) $< -o $@ $(FA_DLIB)

demo: $(APP_OBJS) $(PLG_OBJS)
	$(CC) $(CXXFLAGS) $^ -o $@ $(WRAPS) $(CXXLIBS) $(FA_DLIB)


.PHONY: clean
clean:
	rm -f $(APP_OBJS) $(PLG_OBJS) demo
