TARBALL = ./xz-5.2.1.tar.bz2
SRC_DIR = xz-5.2.1
STAGING_DIR = ./staging

# QSDK build commands (exact replica):
# $(MAKE) $(HOST_JOBS) -C $(HOST_BUILD_DIR) install xzlinks="unxz xzcat"

.PHONY: all extract patch build install clean

all: install

extract:
	@if [ ! -d "$(SRC_DIR)" ]; then \
		echo "Extracting xz tarball..."; \
		tar -xjf $(TARBALL); \
	fi

patch: extract
	@if [ ! -f "$(SRC_DIR)/.patched" ]; then \
		echo "Applying xz patches..."; \
		../apply_patches.sh $(CURDIR)/$(SRC_DIR) $(CURDIR)/patches && \
		touch $(SRC_DIR)/.patched; \
	fi

build: patch
	@echo "Configuring and building xz (QSDK exact)..."
	cd $(SRC_DIR) && ./configure \
		--prefix=$(CURDIR)/$(STAGING_DIR) \
		--disable-shared \
		--enable-static \
		--disable-nls \
		--disable-doc \
		--disable-scripts
	$(MAKE) -C $(SRC_DIR)

install: build
	@echo "Installing xz to staging dir..."
	$(MAKE) -C $(SRC_DIR) install xzlinks="unxz xzcat"

clean:
	@echo "Cleaning xz build environment..."
	rm -rf $(SRC_DIR) $(STAGING_DIR)
