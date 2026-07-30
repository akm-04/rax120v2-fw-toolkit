#
# Copyright (C) 2007-2009 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=net6conf
PKG_VERSION:=0.2
PKG_RELEASE:=1
PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)
PKG_GIT_TREEISH=net6conf-dni120
PKG_BUILD_DEPENDS:=wide-dhcpv6-client wide-dhcpv6-server radvd

include $(INCLUDE_DIR)/package.mk

define Package/net6conf
  SECTION:=dni
  CATEGORY:=DNI Packages
  DEPENDS:=+kmod-ipv6
  TITLE:=IPv6 scripts
  PKGARCH:=all
endef

define Build/Prepare
	$(CP) $(GIT_HOME)/net6conf.git/* $(PKG_BUILD_DIR)/
endef

define Build/Configure
endef

define Build/Compile
endef


define Package/net6conf/install
	$(CP) -rf $(PKG_BUILD_DIR)/Binary/* $(1)/
endef

$(eval $(call BuildPackage,net6conf))
