CC=g++
CXXFLAGS=-std=c++20 -Wall -Werror -D_GNU_SOURCE
CXXLIBS=-pthread

SRCS:=$(wildcard ./*.cpp)
HDRS:=$(wildcard ./*.hpp)
OBJS:=$(patsubst %.cpp,%.o,$(SRCS))


.PHONY: all
all: release


.PHONY: release
release: CXXFLAGS+=-O3 -DNDEBUG
release: libbptree.so

.PHONY: debug
debug: CXXFLAGS+=-Og -g -ggdb
debug: libbptree.so


$(OBJS): %.o: %.cpp $(HDRS)
	$(CC) $(CXXFLAGS) -c -fPIC $< -o $@ $(CXXLIBS)

libbptree.so: $(OBJS)
	$(CC) -shared $(CXXFLAGS) $^ -o $@ $(CXXLIBS)


.PHONY: clean
clean:
	rm -f *.o *.so
