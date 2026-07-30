#
# Copyright (C) 2007-2008 Delta Networks Inc.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=datalib
PKG_VERSION:=1.0
PKG_RELEASE:=1

PKG_GIT_TREEISH:=datalib-dni43
CONFIG_MAGIC=0x20170711
RAND_MAGIC=0x20170711
SPACE_SIZE=0x60000

PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk

define Package/datalib
  SECTION:=utils
  CATEGORY:=DNI Packages
  DEPENDS:=+dni-libmtd
  TITLE:=Small application for saving data on embedded system
endef

define Build/Prepare
	$(CP) $(GIT_HOME)/datalib.git/* $(PKG_BUILD_DIR)/
endef

define Build/Configure
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
	rm -f $(STAGING_DIR)/usr/lib/libconfig.so
endef

define Package/datalib/install
	$(CP) -rf $(PKG_BUILD_DIR)/Binary/* $(1)/
endef

$(eval $(call BuildPackage,datalib))
