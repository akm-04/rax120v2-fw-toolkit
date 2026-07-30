#
# Copyright (C) 2018-2020 Delta Networks Inc.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=ntgrcrypt
PKG_VERSION:=1.0
PKG_RELEASE:=1

PKG_GIT_TREEISH:="ntgrcrypt-dni041"

PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk

define Package/ntgrcrypt
  SECTION:=utils
  CATEGORY:=Utilities
  TITLE:=ntgrcrypt for security sotrage libntgrcrypt.so
endef

define Build/Prepare
	$(CP) $(GIT_HOME)/ntgrcrypt.git/* $(PKG_BUILD_DIR)/
endef

define Build/Compile
endef

define Build/InstallDev
	mkdir -p $(STAGING_DIR)/usr/lib
	mkdir -p $(STAGING_DIR)/usr/include
	mkdir -p $(STAGING_DIR)/lib
	$(CP) -rf $(PKG_BUILD_DIR)/Binary/usr/lib $(STAGING_DIR)/usr/
endef

define Build/UninstallDev
	rm -f $(STAGING_DIR)/usr/lib/libntgrcrypt.so
endef

define Package/ntgrcrypt/install
	openssl rand -base64 2048 >  $(PKG_BUILD_DIR)/files/deif_original.key
	$(PKG_BUILD_DIR)/src/Host-interleave -y -i $(PKG_BUILD_DIR)/files/deif_original.key -o $(PKG_BUILD_DIR)/files/deif.key
	$(INSTALL_DIR) $(1)/sbin
	$(INSTALL_DIR) $(1)/usr/lib
	$(INSTALL_DIR) $(1)/usr/share/deif
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/files/deif.* $(1)/usr/share/deif
	$(CP) $(PKG_BUILD_DIR)/files/Host-encrypt-files.sh $(BUILD_DIR_BASE)/
	$(CP) $(PKG_BUILD_DIR)/files/deif_original.key $(BUILD_DIR_BASE)/
	$(CP) -rf $(PKG_BUILD_DIR)/Binary/* $(1)/
endef

$(eval $(call BuildPackage,ntgrcrypt))
