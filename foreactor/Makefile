CC=g++
CXXFLAGS=-Wall -Werror -std=c++20 -D_GNU_SOURCE

FA_SRCS:=$(wildcard core/*.cpp)
FA_SRCS+=$(wildcard utils/*.cpp)
FA_OBJS:=$(patsubst %.cpp,%.o,$(FA_SRCS))
FA_INCL:=-I./include -I./utils


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


%.o: %.cpp
	$(CC) $(CXXFLAGS) -c -fPIC $(FA_INCL) $< -o $@ -luring -ldl

libforeactor.so: $(FA_OBJS)
	$(CC) $(CXXFLAGS) -fPIC -shared $^ -o $@ -luring -ldl


.PHONY: clean
clean:
	rm -f $(FA_OBJS) lib*.so
