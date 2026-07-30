#
# Copyright(C) 2006-2018 OpenWrt.org
#
include $(TOPDIR)/rules.mk
PKG_NAME:=auto-detection-tz
PKG_VERSION:=1.0
PKG_RELEASE:=1.0

PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk

define Package/auto-detection-tz
    SECTION:=utils
    DEPENDS:=+datalib
    CATEGORY:=DNI Packages
    TITLE:=auto-detection-tz
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Build/Compile
endef

define Package/auto-detection-tz/description
	Auto Detection TimeZone
endef

define Package/auto-detection-tz/install
	$(CP) -rf $(PKG_BUILD_DIR)/Binary/* $(1)/
endef

$(eval $(call BuildPackage,auto-detection-tz))
