COREUTILS_DIR:=coreutils-src

FA_PATH:=../../libforeactor
FA_DLIB:=-L$(FA_PATH) -lforeactor -Wl,-rpath=$(FA_PATH)


.PHONY: all
all: release


.PHONY: release
release: CFLAGS=-O3 -DNDEBUG
release: coreutils

.PHONY: debug
debug: CFLAGS=-Og -g -ggdb
debug: coreutils


.PHONY: coreutils
coreutils:
	cd $(COREUTILS_DIR) && \
		./configure "CFLAGS=$(CFLAGS)" && \
		make -j$(nproc)

.PHONY: build-skipconf
build-skipconf:
	cd $(COREUTILS_DIR) && \
		make -j$(nproc)


.PHONY: clean
clean:
	cd $(COREUTILS_DIR) && \
		make clean && \
		make distclean

.PHONY: clean-skipdist
clean-skipdist:
	cd $(COREUTILS_DIR) && \
		make clean
