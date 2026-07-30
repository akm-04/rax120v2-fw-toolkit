# 
#  Copyright (C) 2007-2008 Delta Networks Inc.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=net-wall
PKG_RELEASE:=2
PKG_BUILD_DEPENDS:=datalib
PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)

PKG_GIT_TREEISH:=net-wall-v2.0-dni107

include $(INCLUDE_DIR)/package.mk

define Package/net-wall
  SECTION:=utils
  CATEGORY:=DNI Packages
  DEPENDS:=+datalib
  TITLE:=Mini Firewall for Embedded Router.
endef

define Build/Prepare
	$(CP) $(GIT_HOME)/net-wall.git/* $(PKG_BUILD_DIR)/
endef

define Build/Compile
endef

define Package/net-wall/install
	$(CP) -rf $(PKG_BUILD_DIR)/Binary/* $(1)/
endef

$(eval $(call BuildPackage,net-wall))
