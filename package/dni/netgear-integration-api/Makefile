# 
#  Copyright (C) 2014 Delta Networks Inc.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=netgear-integration-api
PKG_RELEASE:=1
PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk

define Package/netgear-integration-api
  SECTION:=dni
  CATEGORY:=DNI Packages
  DEPENDS:= +datalib
  TITLE:=Netgear integration API
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Build/Compile
endef

define Package/netgear-integration-api/install
	$(CP) -rf $(PKG_BUILD_DIR)/Binary/* $(1)/
endef

$(eval $(call BuildPackage,netgear-integration-api))
