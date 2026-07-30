# 
# Copyright (C) 2006 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#
# $Id: Makefile,v 1.1.2.3 2007/01/12 02:45:49 ronger Exp $

include $(TOPDIR)/rules.mk

PKG_NAME:=lltd
PKG_VERSION:=0.1
PKG_RELEASE:=1

#PKG_SOURCE:=$(PKG_NAME)-$(PKG_VERSION).tar.gz
#PKG_SOURCE_URL:=$(DNI_DL_WEB) http://gentoo.osuosl.org/distfiles @SF/utelnetd
#PKG_MD5SUM:=45fc2b66b3d2dade396ed122a41d5b23
#PKG_CAT:=zcat

PKG_GIT_TREEISH:=lltd-v1.2-dni3
PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)-$(PKG_VERSION)/native-linux

include $(INCLUDE_DIR)/package.mk

define Package/lltd
  SECTION:=net
  CATEGORY:=DNI Packages
  DEPENDS:=@TARGET_ipq
  TITLE:=Link Layer Topology Discovery Daemon
  URL:=http://lltd.sourceforge.net/
endef

define Build/Prepare
	$(CP) $(GIT_HOME)/lltd.git/* $(BUILD_DIR)/$(PKG_NAME)-$(PKG_VERSION)/
endef

define Build/Compile
endef

define Build/Configure
endef

define Package/lltd/install
	$(CP) -rf $(PKG_BUILD_DIR)/Binary/* $(1)/
endef

$(eval $(call BuildPackage,lltd))
