# 
# Copyright (C) 2006 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#
# $Id: Makefile,v 1.1.2.2 2007/01/31 06:23:51 ronger Exp $

include $(TOPDIR)/rules.mk

PKG_NAME:=miniupnpd
PKG_VERSION:=20070127
PKG_RELEASE:=3

PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk


define Package/miniupnpd
  SECTION:=dni
  CATEGORY:=DNI Packages
  TITLE:=Miniupnpd Daemon
  DEPENDS:= +iptables +libip4tc +IPV6:libip6tc +IPV6:ip6tables
  URL:=http://miniupnp.free.fr/
endef


MAKE_FLAGS += \
        TEST=0 \
        LIBS="" \
        CC="$(TARGET_CC) -lip4tc $(if $(CONFIG_IPV6),-lip6tc)" \
        -f Makefile.linux \
        miniupnpd

define Build/Prepare
	$(CP) $(GIT_HOME)/miniupnpd.git/* $(PKG_BUILD_DIR)/
endef

define Build/Compile
endef

define Build/Configure
endef

define Package/miniupnpd/install
	$(CP) -rf $(PKG_BUILD_DIR)/Binary/* $(1)/
endef

$(eval $(call BuildPackage,miniupnpd))
