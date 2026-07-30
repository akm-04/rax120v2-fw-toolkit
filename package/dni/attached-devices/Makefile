# 
#  Copyright (C) 2007-2009 Delta Networks Inc.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=attached-devices
PKG_RELEASE:=1

PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)
CONFIG_NETSCAN_GIT_REPOSITORY:=attached-devices.git

PKG_GIT_TREEISH="a0ede1acf216a5bbd548f8716da81c3596779501"

include $(INCLUDE_DIR)/package.mk

define Package/attached-devices
  	SECTION:=dni
  	CATEGORY:=DNI Packages
  	DEPENDS:= +datalib +d2 +dil +ntgdal +dniutil
	TITLE:=Scanning the attached devices according to NETGEAR's spec.
endef

TARGET_CFLAGS += -DCONFIG_USE_MUSL -D_GNU_SOURCE -ld2lib -ldillib -lpthread -ldniutil -ld2dynlog

define Build/Prepare
	$(CP) $(GIT_HOME)/attached-devices.git/* $(PKG_BUILD_DIR)/
endef

define Build/Compile
endef

define Package/attached-devices/install
	$(CP) -rf $(PKG_BUILD_DIR)/Binary/* $(1)/
endef

$(eval $(call BuildPackage,attached-devices))
