#
# Copyright (C) 2015 Delta Networks Inc.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=dnitoolbox
PKG_VERSION:=0.1
PKG_RELEASE:=1

PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk

define Package/$(PKG_NAME)
  SECTION:=utils
  CATEGORY:=DNI Packages
  TITLE:=a set of dni common tools
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Build/Compile
endef

define Package/$(PKG_NAME)/install
	$(CP) -rf $(PKG_BUILD_DIR)/Binary/* $(1)/
endef

$(eval $(call BuildPackage,$(PKG_NAME)))
