CC=g++
CXXFLAGS=-Wall -Werror -std=c++20 -D_GNU_SOURCE
CXXLIBS=-luring -ldl -pthread

FA_SRCS:=$(wildcard core/*.cpp)
FA_SRCS+=$(wildcard syscalls/*.cpp)
FA_SRCS+=$(wildcard engine/*.cpp)
FA_SRCS+=$(wildcard utils/*.cpp)

FA_HDRS:=$(wildcard core/*.hpp)
FA_HDRS+=$(wildcard syscalls/*.hpp)
FA_HDRS+=$(wildcard engine/*.hpp)
FA_HDRS+=$(wildcard utils/*.hpp)
FA_HDRS+=$(wildcard include/foreactor.h)

FA_OBJS:=$(patsubst %.cpp,%.o,$(FA_SRCS))

FA_INCL:=-I./core -I./syscalls -I./engine -I./utils -I./include


.PHONY: all
all: release


.PHONY: release
release: CXXFLAGS+=-O3 -DNDEBUG -DNTIMER
release: libforeactor.so

.PHONY: debug
debug: CXXFLAGS+=-Og -g -ggdb -DNTIMER
debug: libforeactor.so

.PHONY: timer
timer: CXXFLAGS+=-O3 -DNDEBUG
timer: libforeactor.so

.PHONY: debug-timer
debug-timer: CXXFLAGS+=-O3
debug-timer: libforeactor.so


%.o: %.cpp $(FA_HDRS)
	$(CC) $(CXXFLAGS) -c -fPIC $(FA_INCL) $< -o $@ $(CXXLIBS)

libforeactor.so: $(FA_OBJS)
	$(CC) $(CXXFLAGS) -fPIC -shared $^ -o $@ $(CXXLIBS)


.PHONY: clean
clean:
	rm -f $(FA_OBJS) lib*.so
