# 
# Copyright (C) 2006 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#
# $Id: Makefile,v 1.1.4.3 2007/02/27 03:06:36 ronger Exp $

include $(TOPDIR)/rules.mk

PKG_NAME:=ntpclient
PKG_RELEASE:=4
PKG_BUILD_DEPENDS:=datalib
PKG_BUILD_DIR := $(BUILD_DIR)/$(PKG_NAME)

PKG_GIT_TREEISH=573218fa28d5d0de6e5ba5d86f8e0a07849fb141

include $(INCLUDE_DIR)/package.mk

define Package/dni-ntpclient
  SECTION:=dni
  CATEGORY:=DNI Packages
  #DEPENDS:=@TARGET_ipq806x
  DEPENDS:= +datalib
  TITLE:=The CGI interface
endef

define Package/dni-ntpclient/description
	The CGI interface
endef

define Build/Prepare
	$(CP) $(GIT_HOME)/ntpclient.git/* $(PKG_BUILD_DIR)/
endef

define Package/dni-ntpclient/install
	install -d -m0755 $(1)/usr/sbin
	install -m0755 $(PKG_BUILD_DIR)/src/ntpclient $(1)/usr/sbin/
	install -d -m0755 $(1)/etc/init.d
	install -m0755 $(PKG_BUILD_DIR)/files/ntpclient.init $(1)/etc/init.d/ntpclient
	install -m0755 $(PKG_BUILD_DIR)/files/ntp_updated.sh $(1)/usr/sbin/ntp_updated
endef

$(eval $(call BuildPackage,dni-ntpclient))
