# 
# Copyright (C) 2006 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#
# $Id: Makefile,v 1.1.2.2 2007/02/06 13:52:33 anderson Exp $

include $(TOPDIR)/rules.mk

PKG_GIT_TREEISH="trafficmeter-v2.0.0-dni60"

PKG_NAME:=traffic_meter
PKG_RELEASE:=1

PKG_BUILD_DIR := $(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk

define Package/trafficmeter
  SECTION:=utils
  CATEGORY:=DNI Packages
  DEPENDS:=+datalib
  TITLE:=Update utility for trx firmware images
endef

define Build/Prepare
	$(CP) $(GIT_HOME)/trafficmeter.git/* $(PKG_BUILD_DIR)/
endef

define Build/Compile
endef

define Package/trafficmeter/install
	$(CP) -rf $(PKG_BUILD_DIR)/Binary/* $(1)/
endef

$(eval $(call BuildPackage,trafficmeter))
