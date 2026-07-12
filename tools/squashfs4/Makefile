TARBALL = ./squashfs4.2.tar.gz
SRC_DIR = squashfs4.2
BIN_DIR = ../../bin

# QSDK build commands (exact replica):
# $(MAKE) -C $(HOST_BUILD_DIR)/squashfs-tools \
#     CC="$(HOSTCC)" \
#     XZ_SUPPORT=1 \
#     LZMA_XZ_SUPPORT=1 \
#     XATTR_SUPPORT= \
#     LZMA_LIB="$(STAGING_DIR_HOST)/lib/liblzma.a" \
#     EXTRA_CFLAGS="-I$(STAGING_DIR_HOST)/include" \
#     mksquashfs unsquashfs

XZ_PREFIX = $(CURDIR)/../xz/staging

.PHONY: all extract patch build install clean

all: install

extract:
	@if [ ! -d "$(SRC_DIR)" ]; then \
		echo "Extracting squashfs4.2 tarball..."; \
		tar -xzf $(TARBALL); \
	fi

patch: extract
	@if [ ! -f "$(SRC_DIR)/.patched" ]; then \
		echo "Applying QSDK patches..."; \
		../apply_patches.sh $(CURDIR)/$(SRC_DIR) $(CURDIR)/patches && \
		touch $(SRC_DIR)/.patched; \
	fi

build: patch
	@echo "Compiling squashfs4 tools (QSDK exact)..."
	$(MAKE) -C $(SRC_DIR)/squashfs-tools \
		CC="$(CC)" \
		XZ_SUPPORT=1 \
		LZMA_XZ_SUPPORT=1 \
		XATTR_SUPPORT= \
		LZMA_LIB="$(XZ_PREFIX)/lib/liblzma.a" \
		EXTRA_CFLAGS="-I$(XZ_PREFIX)/include" \
		mksquashfs unsquashfs

install: build
	@echo "Installing squashfs4 tools to $(BIN_DIR)..."
	mkdir -p $(BIN_DIR)
	cp $(SRC_DIR)/squashfs-tools/mksquashfs $(BIN_DIR)/mksquashfs4
	cp $(SRC_DIR)/squashfs-tools/unsquashfs $(BIN_DIR)/unsquashfs4

clean:
	@echo "Cleaning squashfs4 build environment..."
	rm -rf $(SRC_DIR)
