# 
# Copyright (C) 2013 Delta Networks Inc.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=ntgrddns
PKG_VERSION:=1
PKG_RELEASE:=1
PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)
PKG_GIT_TREEISH=ntgrddns-dni15

include $(INCLUDE_DIR)/package.mk

define Package/ntgrddns
  SECTION:=utils
  CATEGORY:=DNI Packages
  DEPENDS:= +datalib
  #DEPENDS:=@TARGET_ipq806x
  TITLE:=Tiny ddns util for Netgear Home Router
endef

define Package/ntgrddns/description
	This package contains a tiny ddns util for Netgear Home Router.
endef

define Build/Prepare
	$(CP) $(GIT_HOME)/ntgrddns.git/* $(PKG_BUILD_DIR)/
endef

define Build/Compile
endef

define Package/ntgrddns/install
	$(CP) -rf $(PKG_BUILD_DIR)/Binary/* $(1)/
endef

$(eval $(call BuildPackage,ntgrddns))
