TARBALL = ./lzma-4.32.tar.bz2
SRC_DIR = lzma-4.32
STAGING_DIR = ./staging

# QSDK build commands (exact replica):
# $(MAKE) -C $(LIB_DIR)
# $(MAKE) -f makefile.gcc -C $(ALONE_DIR)
# $(INSTALL_DATA) $(LIB_DIR)/liblzma.a $(STAGING_DIR_HOST)/lib/liblzma-old.a

LIB_DIR = $(SRC_DIR)/C/7zip/Compress/LZMA_Lib
ALONE_DIR = $(SRC_DIR)/C/7zip/Compress/LZMA_Alone

.PHONY: all extract patch build install clean

all: install

extract:
	@if [ ! -d "$(SRC_DIR)" ]; then \
		echo "Extracting lzma-old tarball..."; \
		tar -xjf $(TARBALL); \
	fi

patch: extract
	@if [ ! -f "$(SRC_DIR)/.patched" ]; then \
		echo "Applying lzma-old patches..."; \
		../apply_patches.sh $(CURDIR)/$(SRC_DIR) $(CURDIR)/patches && \
		touch $(SRC_DIR)/.patched; \
	fi

build: patch
	@echo "Compiling lzma-old (QSDK exact)..."
	$(MAKE) -C $(LIB_DIR)
	$(MAKE) -f makefile.gcc -C $(ALONE_DIR)

install: build
	@echo "Staging lzma-old library for dependents..."
	mkdir -p $(STAGING_DIR)/lib
	cp $(LIB_DIR)/liblzma.a $(STAGING_DIR)/lib/liblzma-old.a

clean:
	@echo "Cleaning lzma-old build environment..."
	rm -rf $(SRC_DIR) $(STAGING_DIR)
