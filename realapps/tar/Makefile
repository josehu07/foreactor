TAR_DIR:=tar-src

FA_PATH:=../../libforeactor
FA_DLIB:=-L$(FA_PATH) -lforeactor -Wl,-rpath=$(FA_PATH)


.PHONY: all
all: release


.PHONY: release
release: CFLAGS=-O3 -DNDEBUG
release: tar

.PHONY: debug
debug: CFLAGS=-Og -g -ggdb
debug: tar


.PHONY: tar
tar:
	cd $(TAR_DIR) && \
		./configure "CFLAGS=$(CFLAGS)" && \
		make -j$(nproc)

.PHONY: build-skipconf
build-skipconf:
	cd $(TAR_DIR) && \
		make -j$(nproc)


.PHONY: clean
clean:
	cd $(TAR_DIR) && \
		make clean && \
		make distclean

.PHONY: clean-skipdist
clean-skipdist:
	cd $(TAR_DIR) && \
		make clean
