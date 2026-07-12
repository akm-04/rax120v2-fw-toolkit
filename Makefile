# Builds tools folder

CC  ?= gcc
CXX ?= g++

export CC CXX

.PHONY: all tools clean

all: tools

tools:
	@echo "Building lzma standalone tool..."
	$(MAKE) -C tools/lzma
	@echo "Building lzma-old library (for squashfs3)..."
	$(MAKE) -C tools/lzma-old
	@echo "Building xz (liblzma for squashfs4)..."
	$(MAKE) -C tools/xz
	@echo "Building squashfs3 tools..."
	$(MAKE) -C tools/squashfs
	@echo "Building squashfs4 tools..."
	$(MAKE) -C tools/squashfs4


clean:
	$(MAKE) -C tools/lzma clean
	$(MAKE) -C tools/lzma-old clean
	$(MAKE) -C tools/xz clean
	$(MAKE) -C tools/squashfs clean
	$(MAKE) -C tools/squashfs4 clean
