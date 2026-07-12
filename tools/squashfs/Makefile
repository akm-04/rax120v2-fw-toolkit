TARBALL = ./squashfs3.0.tar.gz
SRC_DIR = squashfs3.0
BIN_DIR = ../../bin

# QSDK build commands (exact replica):
# $(MAKE) -C $(HOST_BUILD_DIR)/squashfs-tools \
#     CC="$(HOSTCC)" \
#     CXX="$(CXX)" \
#     LZMAPATH=$(STAGING_DIR_HOST)/lib \
#     mksquashfs-lzma unsquashfs-lzma
# $(INSTALL_BIN) $(HOST_BUILD_DIR)/squashfs-tools/mksquashfs-lzma $(STAGING_DIR_HOST)/bin/
# $(INSTALL_BIN) $(HOST_BUILD_DIR)/squashfs-tools/unsquashfs-lzma $(STAGING_DIR_HOST)/bin/

LZMA_OLD_PREFIX = $(CURDIR)/../lzma-old/staging

.PHONY: all extract patch build install clean

all: install

extract:
	@if [ ! -d "$(SRC_DIR)" ]; then \
		echo "Extracting squashfs3.0 tarball..."; \
		tar -xzf $(TARBALL); \
	fi

patch: extract
	@if [ ! -f "$(SRC_DIR)/.patched" ]; then \
		echo "Applying QSDK patches..."; \
		../apply_patches.sh $(CURDIR)/$(SRC_DIR) $(CURDIR)/patches && \
		touch $(SRC_DIR)/.patched; \
	fi

build: patch
	@echo "Compiling squashfs3 tools (QSDK exact)..."
	$(MAKE) -C $(SRC_DIR)/squashfs-tools \
		CC="$(CC)" \
		CXX="$(CXX)" \
		LZMAPATH=$(LZMA_OLD_PREFIX)/lib \
		mksquashfs-lzma unsquashfs-lzma

install: build
	@echo "Installing squashfs3 tools to $(BIN_DIR)..."
	mkdir -p $(BIN_DIR)
	cp $(SRC_DIR)/squashfs-tools/mksquashfs-lzma $(BIN_DIR)/mksquashfs-lzma
	cp $(SRC_DIR)/squashfs-tools/unsquashfs-lzma $(BIN_DIR)/unsquashfs-lzma

clean:
	@echo "Cleaning squashfs3 build environment..."
	rm -rf $(SRC_DIR)
