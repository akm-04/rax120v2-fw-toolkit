#
# Copyright (C) 2015 OpenWrt.org
#

include $(TOPDIR)/rules.mk

PKG_NAME:=mapcalc
PKG_RELEASE:=1
PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk

define Package/mapcalc
  SECTION:=net
  CATEGORY:=DNI Packages
  DEPENDS:=+libubox +libubus
  TITLE:=IPv6 MAP-E calc tool
endef

TARGET_CFLAGS += -std=gnu99
TARGET_CFLAGS += -I$(STAGING_DIR)/usr/include

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)
endef


define Package/mapcalc/install
	$(INSTALL_DIR) $(1)/etc/init.d
	#$(CP) ./files/* $(1)/etc/init.d
	install -d -m0755 $(1)/usr/sbin
	install -m0755 $(PKG_BUILD_DIR)/mapcalc $(1)/usr/sbin/
endef

$(eval $(call BuildPackage,mapcalc))
