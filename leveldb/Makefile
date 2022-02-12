CC=g++
CFLAGS_TRACING=-O1 -g -ggdb -I./leveldb-1.23/include -gpubnames -fno-omit-frame-pointer \
               -fkeep-inline-functions -fkeep-static-functions
CFLAGS_RELEASE=-O2 -I./leveldb-1.23/include
CFLAGS_EXTRA=-Wall -Werror
CSTATIC=./leveldb-1.23/build/libleveldb.a
CDYLIBS=-lpthread

TARGET=ycsbcli


.PHONY: all tracing release
all: tracing


tracing: $(TARGET).cpp cxxopts.hpp
	$(CC) $(CFLAGS_TRACING) $(CFLAGS_EXTRA) -o $(TARGET) $^ $(CSTATIC) $(CDYLIBS)

release: $(TARGET).cpp cxxopts.hpp
	$(CC) $(CFLAGS_RELEASE) $(CFLAGS_EXTRA) -o $(TARGET) $^ $(CSTATIC) $(CDYLIBS)


.PHONY: clean
clean:
	rm -f $(TARGET)
