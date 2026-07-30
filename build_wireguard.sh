#!/usr/bin/env bash
# build.sh – OpenWrt WireGuard build script for RAX120
# Run from the root of the OpenWrt build tree.

set -e          # exit on any error
set -u          # treat unset variables as errors

# ======================================================================
echo "==> Cloning broken/missing links from QSDK source..."
# Any clone / wget error during build must be resolved manually
mkdir -p dl
wget -O dl/gmp-5.1.3.tar.xz https://ftp.gnu.org/gnu/gmp/gmp-5.1.3.tar.xz
wget -O dl/e2fsprogs-1.42.8.tar.gz https://mirrors.edge.kernel.org/pub/linux/kernel/people/tytso/e2fsprogs/v1.42.8/e2fsprogs-1.42.8.tar.gz

# ======================================================================
echo "==> Setting up wireguard makefiles..."
mkdir -p package/feeds/wireguard/wireguard
mkdir -p package/feeds/wireguard/wireguard-tools

# kmod-wireguard Makefile
{
echo 'include $(TOPDIR)/rules.mk'
echo 'include $(INCLUDE_DIR)/kernel.mk'
echo ''
echo 'PKG_NAME:=wireguard'
echo 'PKG_VERSION:=1.0.20200520'
echo 'PKG_RELEASE:=1'
echo ''
echo 'PKG_SOURCE_PROTO:=git'
echo 'PKG_SOURCE_URL:=https://github.com/WireGuard/wireguard-linux-compat.git'
echo 'PKG_SOURCE_VERSION:=v1.0.20200520'
echo 'PKG_SOURCE_SUBDIR:=$(PKG_NAME)-$(PKG_VERSION)'
echo 'PKG_SOURCE:=$(PKG_NAME)-$(PKG_VERSION).tar.xz'
echo ''
echo 'PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)-$(PKG_VERSION)'
echo ''
echo 'include $(INCLUDE_DIR)/package.mk'
echo ''
echo 'define Build/Prepare'
printf '\t$(call Build/Prepare/Default)\n'
printf '\t# Patch out functions QSDK kernel already provides to avoid redefinition errors\n'
printf '\tsed -i "s/static inline void \\*skb_put_data/static inline void \\*wg_skb_put_data_unused/" $(PKG_BUILD_DIR)/src/compat/compat.h\n'
echo 'endef'
echo ''
echo 'define KernelPackage/wireguard'
echo '  SUBMENU:=Network Support'
echo '  TITLE:=WireGuard kernel module'
echo '  DEPENDS:=+kmod-crypto-core +kmod-crypto-cmac +kmod-crypto-hmac +kmod-crypto-blkcipher'
echo '  FILES:=$(PKG_BUILD_DIR)/src/wireguard.ko'
echo '  AUTOLOAD:=$(call AutoProbe,wireguard)'
echo 'endef'
echo ''
echo 'define Build/Compile'
printf '\t$(MAKE) -C $(LINUX_DIR) \\\n'
printf '\t\tARCH="$(LINUX_KARCH)" \\\n'
printf '\t\tCROSS_COMPILE="$(TARGET_CROSS)" \\\n'
printf '\t\tSUBDIRS="$(PKG_BUILD_DIR)/src" \\\n'
printf '\t\tmodules\n'
echo 'endef'
echo ''
echo '$(eval $(call KernelPackage,wireguard))'
} > package/feeds/wireguard/wireguard/Makefile

# wg utility Makefile
{
echo 'include $(TOPDIR)/rules.mk'
echo ''
echo 'PKG_NAME:=wireguard-tools'
echo 'PKG_VERSION:=1.0.20200513'
echo 'PKG_RELEASE:=1'
echo ''
echo 'PKG_SOURCE_PROTO:=git'
echo 'PKG_SOURCE_URL:=https://github.com/WireGuard/wireguard-tools.git'
echo 'PKG_SOURCE_VERSION:=v1.0.20200513'
echo 'PKG_SOURCE_SUBDIR:=$(PKG_NAME)-$(PKG_VERSION)'
echo 'PKG_SOURCE:=$(PKG_NAME)-$(PKG_VERSION).tar.xz'
echo ''
echo 'PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)-$(PKG_VERSION)'
echo ''
echo 'include $(INCLUDE_DIR)/package.mk'
echo ''
echo 'define Package/wireguard-tools'
echo '  SECTION:=net'
echo '  CATEGORY:=Network'
echo '  SUBMENU:=VPN'
echo '  TITLE:=WireGuard userspace tools'
echo '  DEPENDS:=+libmnl +kmod-wireguard'
echo 'endef'
echo ''
echo 'define Build/Prepare'
printf '\t$(call Build/Prepare/Default)\n'
printf '\tmkdir -p $(PKG_BUILD_DIR)/src/linux/\n'
printf '\tcp $(CURDIR)/wireguard.h $(PKG_BUILD_DIR)/src/linux/\n'
printf '\tsed -i "s|#define SOCK_PATH RUNSTATEDIR \\"/wireguard/\\"|#define SOCK_PATH \\"/var/run/wireguard/\\"|g" $(PKG_BUILD_DIR)/src/ipc.c\n'
echo 'endef'
echo ''
echo 'define Build/Compile'
printf '\t$(MAKE) -C $(PKG_BUILD_DIR)/src \\\n'
printf '\t\tCC="$(TARGET_CC)" \\\n'
printf '\t\tCFLAGS="$(TARGET_CFLAGS) -std=gnu11 -D_GNU_SOURCE -I$(PKG_BUILD_DIR)/src/" \\\n'
printf '\t\tLDFLAGS="$(TARGET_LDFLAGS)" \\\n'
printf '\t\tWITH_BASHCOMPLETION=no \\\n'
printf '\t\tWITH_WGQUICK=no \\\n'
printf '\t\tWITH_SYSTEMDUNITS=no \\\n'
printf '\t\tDESTDIR="$(PKG_INSTALL_DIR)" \\\n'
printf '\t\tinstall\n'
echo 'endef'
echo ''
echo 'define Package/wireguard-tools/install'
printf '\t$(INSTALL_DIR) $(1)/usr/bin\n'
printf '\t$(INSTALL_BIN) $(PKG_INSTALL_DIR)/usr/bin/wg $(1)/usr/bin/\n'
echo 'endef'
echo ''
echo '$(eval $(call BuildPackage,wireguard-tools))'
} > package/feeds/wireguard/wireguard-tools/Makefile

# ======================================================================
echo "==> Copying default configuration for RAX120..."
cp configs/defconfig-rax120-hk2.0 .config

echo "==> Enabling wireguard in .config ..."
echo "CONFIG_PACKAGE_wireguard=m" >> .config
echo "CONFIG_PACKAGE_kmod-wireguard=m" >> .config
echo "CONFIG_PACKAGE_wireguard-tools=m" >> .config

# ======================================================================
# Always use -j1 to avoid compilation error due to multiprocessing.
echo "==> Building tools..."
make tools/install -j1 V=99

echo "==> Building toolchain..."
make toolchain/install -j1 V=99

echo "==> Compiling kernel..."
make target/linux/compile -j1 V=99

echo "==> Cleaning WireGuard kernel module..."
make package/feeds/wireguard/wireguard/clean V=99 -j1

echo "==> Compiling WireGuard kernel module..."
make package/feeds/wireguard/wireguard/compile V=99 -j1

# Ensure the wireguard.h header is present in the package folder
# This prevents build failures if the repo was freshly cloned
if [ ! -s "package/feeds/wireguard/wireguard-tools/wireguard.h" ]; then
    echo "==> Extracting wireguard.h header..."
    mkdir -p /tmp/wg-extract
    tar -xJf dl/wireguard-1.0.20200520.tar.xz -C /tmp/wg-extract
    find /tmp/wg-extract/ -name "wireguard.h" -exec cp {} package/feeds/wireguard/wireguard-tools/wireguard.h \;
fi

echo "==> Cleaning WireGuard userspace tools..."
make package/feeds/wireguard/wireguard-tools/clean V=99 -j1

echo "==> Compiling WireGuard userspace tools..."
make package/feeds/wireguard/wireguard-tools/compile V=99 -j1

# ======================================================================
echo "==> Collecting IPK files..."
find bin/ -iname "*wireguard*"

echo "==> Extracting raw binaries from IPKs..."
mkdir -p bin/wireguard
TMP_EXTRACT=$(mktemp -d)

# Dynamically locate the built IPKs
KMOD_IPK=$(find bin/ -name "kmod-wireguard_*.ipk" | head -n 1)
TOOLS_IPK=$(find bin/ -name "wireguard-tools_*.ipk" | head -n 1)

if [ -n "$KMOD_IPK" ] && [ -n "$TOOLS_IPK" ]; then
    # Create separate extraction folders
    mkdir -p "$TMP_EXTRACT/kmod" "$TMP_EXTRACT/tools"
    
    # Extract kmod IPK fully, then unpack its data.tar.gz payload
    tar -xzf "$KMOD_IPK" -C "$TMP_EXTRACT/kmod/"
    tar -xzf "$TMP_EXTRACT/kmod/data.tar.gz" -C "$TMP_EXTRACT/kmod/"
    
    # Extract tools IPK fully, then unpack its data.tar.gz payload
    tar -xzf "$TOOLS_IPK" -C "$TMP_EXTRACT/tools/"
    tar -xzf "$TMP_EXTRACT/tools/data.tar.gz" -C "$TMP_EXTRACT/tools/"

    # Move the necessary files to the final destination
    cp "$TMP_EXTRACT/kmod/lib/modules/4.4.60/wireguard.ko" bin/wireguard/
    cp "$TMP_EXTRACT/tools/usr/bin/wg" bin/wireguard/

    echo "==> Necessary files extracted to bin/wireguard/:"
    ls -lh bin/wireguard/

    # Clean up the temp directory
    rm -rf "$TMP_EXTRACT"
else
    echo "❌ Error: Could not find IPK files to extract."
    exit 1
fi

echo "✅ All tasks completed successfully."
