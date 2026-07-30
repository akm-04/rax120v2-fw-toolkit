# 
# Copyright (C) 2008 OpenWrt.org
#

include $(TOPDIR)/rules.mk

PKG_NAME:=wlan-common
PKG_VERSION:=2.5
PKG_RELEASE:=1

PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/kernel.mk

define Package/wlan-common
  SECTION:=net
  CATEGORY:=Network
  TITLE:=DNI wlan common scripts
endef

PLATFORM_PATCH_DIR:=./patches-$(BOARD)


define Build/Prepare
	$(CP) $(GIT_HOME)/wlan-common.git/* $(PKG_BUILD_DIR)/
endef

define Build/Compile
endef

define Package/wlan-common/install
	$(CP) -rf $(PKG_BUILD_DIR)/Binary/* $(1)/
endef

$(eval $(call BuildPackage,wlan-common))
