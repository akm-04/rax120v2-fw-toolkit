TARBALL = ./lzma-4.65.tar.bz2
SRC_DIR = lzma-4.65
BIN_DIR = ../../bin
STAGING_DIR = ./staging

# QSDK build commands (exact replica):
# $(MAKE) -C $(UTIL_DIR) -f makefile.gcc
# $(MAKE) -C $(ALONE_DIR) -f makefile.gcc
# $(INSTALL_BIN) $(HOST_BUILD_DIR)/CPP/7zip/Compress/LZMA_Alone/lzma_alone $(STAGING_DIR_HOST)/bin/lzma

UTIL_DIR = $(SRC_DIR)/C/LzmaUtil
ALONE_DIR = $(SRC_DIR)/CPP/7zip/Compress/LZMA_Alone

.PHONY: all extract patch build install clean

all: install

extract:
	@if [ ! -d "$(SRC_DIR)" ]; then \
		echo "Extracting lzma tarball..."; \
		tar -xjf $(TARBALL); \
	fi

patch: extract
	@if [ ! -f "$(SRC_DIR)/.patched" ]; then \
		echo "Applying lzma patches..."; \
		../apply_patches.sh $(CURDIR)/$(SRC_DIR) $(CURDIR)/patches && \
		touch $(SRC_DIR)/.patched; \
	fi

build: patch
	@echo "Compiling lzma (QSDK exact)..."
	$(MAKE) -C $(UTIL_DIR) -f makefile.gcc
	$(MAKE) -C $(ALONE_DIR) -f makefile.gcc

install: build
	@echo "Installing lzma binary to $(BIN_DIR)..."
	mkdir -p $(BIN_DIR)
	cp $(ALONE_DIR)/lzma_alone $(BIN_DIR)/lzma
	@echo "Staging lzma library and headers for dependents..."
	mkdir -p $(STAGING_DIR)/lib $(STAGING_DIR)/include
	cp $(UTIL_DIR)/liblzma.a $(STAGING_DIR)/lib/liblzma.a
	cp $(SRC_DIR)/C/LzmaLib.h $(STAGING_DIR)/include/
	cp $(SRC_DIR)/C/LzmaDec.h $(STAGING_DIR)/include/
	cp $(SRC_DIR)/C/LzmaEnc.h $(STAGING_DIR)/include/
	cp $(SRC_DIR)/C/Alloc.h   $(STAGING_DIR)/include/
	cp $(SRC_DIR)/C/Types.h   $(STAGING_DIR)/include/

clean:
	@echo "Cleaning lzma build environment..."
	rm -rf $(SRC_DIR) $(STAGING_DIR)
