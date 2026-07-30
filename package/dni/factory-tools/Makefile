#
# Copyright (C) 2012-2013 Delta Networks Inc.
#    

include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk

PKG_NAME:=factory-tools
PKG_RELEASE:=1

PKG_BUILD_DIR := $(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk

define Package/factory-tools
  SECTION:=dni
  CATEGORY:=DNI Packages
  DEPENDS:= +datalib
  TITLE:=Test utility for DNI factory
endef

define Package/factory-tools/description
 This package contains some utility useful for DNI factory to test the product
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Build/Compile
endef


define Package/factory-tools/install
	$(CP) -rf $(PKG_BUILD_DIR)/Binary/* $(1)/
endef

$(eval $(call BuildPackage,factory-tools))
