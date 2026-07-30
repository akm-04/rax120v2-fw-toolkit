# 
#  Copyright (C) 2007-2009 Delta Networks Inc.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=dni-acl
PKG_RELEASE:=1

PKG_GIT_TREEISH="a9680516e402ec1d9477e1bf8b7890ba283f3d4a"

PKG_BUILD_DEPENDS:=datalib
PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk

define Package/dni-acl
  	SECTION:=dni
	CATEGORY:=DNI Packages
	TITLE:=Manage ACL.
	DEPENDS:= +datalib
endef

define Build/Prepare
	$(CP) $(GIT_HOME)/dni-acl.git/* $(PKG_BUILD_DIR)/
endef

define Build/Compile
endef
define Package/dni-acl/install
	$(CP) -rf $(PKG_BUILD_DIR)/Binary/* $(1)/
endef

$(eval $(call BuildPackage,dni-acl))
