include $(TOPDIR)/rules.mk
#include $(INCLUDE_DIR)/nls.mk
#include $(INCLUDE_DIR)/target.mk

PKG_NAME:=d2
PKG_VERSION:=0.1.0
PKG_RELEASE:=1

#PKG_BUILD_DIR:=./files/

include $(INCLUDE_DIR)/package.mk

define Package/d2
  SECTION:=d2
  CATEGORY:=Netgear
  TITLE:=D2
  DEPENDS:=+libpthread +libnghttp2 +librt +libc +zlib +ld2dynlog
endef


define Package/d2/description
 Netgear Data Abstraction Layer 2
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Build/Compile
endef

define Build/InstallDev
	mkdir -p $(STAGING_DIR)/usr/lib
	mkdir -p $(STAGING_DIR)/usr/include
	mkdir -p $(STAGING_DIR)/lib
	$(CP) -rf $(PKG_BUILD_DIR)/Binary/usr/lib $(STAGING_DIR)/usr/
endef

define Package/d2/install
	$(CP) -rf $(PKG_BUILD_DIR)/Binary/* $(1)/
endef


$(eval $(call BuildPackage,d2))
