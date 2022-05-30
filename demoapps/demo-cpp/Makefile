CC=g++
CXXFLAGS=-Wall -Werror -std=c++20

APP_SRCS:=$(wildcard ./demo-cpp-src/*.cpp)
APP_OBJS:=$(patsubst %.cpp,%.o,$(APP_SRCS))
APP_INCL:=-I./demo-cpp-src

PLG_SRCS:=$(wildcard ./demo-cpp-plg/*.cpp)
PLG_OBJS:=$(patsubst %.cpp,%.o,$(PLG_SRCS))

FA_PATH:=../../libforeactor
FA_INCL:=-I$(FA_PATH)/include
FA_DLIB:=-L$(FA_PATH) -lforeactor -Wl,-rpath=$(FA_PATH)

WRAPS:=-Wl,--wrap=_Z12exper_simplePv -Wl,--wrap=_Z15exper_branchingPv -Wl,--wrap=_Z13exper_loopingPv
WRAPS+=-Wl,--wrap=_Z14exper_read_seqPv -Wl,--wrap=_Z15exper_write_seqPv


.PHONY: all
all: release


.PHONY: release
release: CXXFLAGS+=-O3 -DNDEBUG
release: demo

.PHONY: debug
debug: CXXFLAGS+=-Og -g -ggdb
debug: demo


$(APP_OBJS): %.o: %.cpp
	$(CC) $(CXXFLAGS) -c $< -o $@

$(PLG_OBJS): %.o: %.cpp
	$(CC) $(CXXFLAGS) -c $(APP_INCL) $(FA_INCL) $< -o $@ $(FA_DLIB)

demo: $(APP_OBJS) $(PLG_OBJS)
	$(CC) $(CXXFLAGS) $^ -o $@ $(WRAPS) $(FA_DLIB)


.PHONY: clean
clean:
	rm -f $(APP_OBJS) $(PLG_OBJS) demo
