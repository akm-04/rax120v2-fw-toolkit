#
# Copyright (C) 2007-2016 OpenWrt.org
# Copyright (C) 2010 Vertical Communications
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk
include $(INCLUDE_DIR)/kernel.mk
include $(INCLUDE_DIR)/version.mk

PKG_NAME:=base-files_dni
PKG_RELEASE:=157.2

include $(INCLUDE_DIR)/package.mk

define Package/base-files_dni
  SECTION:=base
  CATEGORY:=Base system
  DEPENDS:=+base-files +libpthread +libstdcpp +openssl
  TITLE:=Base filesystem for OpenWrt
  URL:=http://openwrt.org/
  VERSION:=$(PKG_RELEASE)-$(REVISION)
endef

define Package/base-files_dni/description
 This package contains a base filesystem and system scripts for OpenWrt.
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Build/Compile
endef

define Package/base-files_dni/install
	$(CP) -rf $(PKG_BUILD_DIR)/Binary/* $(1)/
endef

$(eval $(call BuildPackage,base-files_dni))
