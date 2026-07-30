#
# Copyright (C) 2007-2008 Delta Networks Inc.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=dniutil
PKG_VERSION:=1.0
PKG_RELEASE:=1

PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)

LOCAL_PKG_NAME_EXT:=dniutil

include $(INCLUDE_DIR)/package.mk

define Package/dniutil
  SECTION:=utils
  CATEGORY:=Utilities
  DEPENDS:=+datalib
  TITLE:=DNI common function library.
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
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
	rm -f $(STAGING_DIR)/usr/lib/libdniutil.so
endef

define Package/dniutil/install
	$(CP) -rf $(PKG_BUILD_DIR)/Binary/* $(1)/
endef

$(eval $(call BuildPackage,dniutil))
