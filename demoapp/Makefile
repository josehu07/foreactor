CC=g++
CXXFLAGS=-Wall -Werror -std=c++20 -O3 -DNDEBUG

APP_SRCS:=$(wildcard ./src/*.cpp)
APP_OBJS:=$(patsubst %.cpp,%.o,$(APP_SRCS))
APP_INCL:=-I./src

PLG_SRCS:=$(wildcard ./plugin/*.cpp)
PLG_OBJS:=$(patsubst %.cpp,%.o,$(PLG_SRCS))

FA_INCL:=-I../foreactor/include
FA_DLIB:=-L../foreactor -lforeactor -Wl,-rpath=../foreactor

WRAPS:=-Wl,--wrap=_Z7ldb_getB5cxx11RSt6vectorIS_IiSaIiEESaIS1_EE


.PHONY: all
all: demoapp


$(APP_OBJS): %.o: %.cpp
	$(CC) $(CXXFLAGS) -c $< -o $@

$(PLG_OBJS): %.o: %.cpp
	$(CC) $(CXXFLAGS) -c $(APP_INCL) $(FA_INCL) $< -o $@ $(FA_DLIB)

demoapp: $(APP_OBJS) $(PLG_OBJS)
	$(CC) $(CXXFLAGS) $^ -o $@ $(WRAPS) $(FA_DLIB)


.PHONY: clean
clean:
	rm -f $(APP_OBJS) $(PLG_OBJS) demoapp
