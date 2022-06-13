CC=gcc
CFLAGS=-Wall -Werror -std=c17
CLIBS=-luring -pthread

APP_SRCS:=$(wildcard ./demo-c-src/*.c)
APP_OBJS:=$(patsubst %.c,%.o,$(APP_SRCS))
APP_INCL:=-I./demo-c-src

PLG_SRCS:=$(wildcard ./demo-c-plg/*.c)
PLG_OBJS:=$(patsubst %.c,%.o,$(PLG_SRCS))

FA_PATH:=../../libforeactor
FA_INCL:=-I$(FA_PATH)/include
FA_DLIB:=-L$(FA_PATH) -lforeactor -Wl,-rpath=$(FA_PATH)

WRAPS:=-Wl,--wrap=exper_simple


.PHONY: all
all: release


.PHONY: release
release: CFLAGS+=-O3 -DNDEBUG
release: demo

.PHONY: debug
debug: CFLAGS+=-Og -g -ggdb
debug: demo


$(APP_OBJS): %.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@ $(CLIBS)

$(PLG_OBJS): %.o: %.c
	$(CC) $(CFLAGS) -c $(APP_INCL) $(FA_INCL) $< -o $@ $(FA_DLIB)

demo: $(APP_OBJS) $(PLG_OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(WRAPS) $(CLIBS) $(FA_DLIB)


.PHONY: clean
clean:
	rm -f $(APP_OBJS) $(PLG_OBJS) demo
