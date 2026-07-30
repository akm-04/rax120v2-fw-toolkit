include $(TOPDIR)/rules.mk

PKG_NAME:=lib-dni-common-api
PKG_VERSION:=0.1
PKG_RELEASE:=1
PKG_GIT_TREEISH:=libdni-common-api-V0.8

PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk


define Package/lib-dni-common-api
  SECTION:=libs
  CATEGORY:=Libraries
  DEPENDS:=+safeclib
  TITLE:=DNI common api module
endef


define Build/Prepare
	$(CP) $(GIT_HOME)/libdni-common-api.git/* $(PKG_BUILD_DIR)/
endef



define Package/lib-dni-common-api/description
 This package contains a library for dni common api module.
endef

#define Build/Prepare
#	mkdir -p $(PKG_BUILD_DIR)
#	$(CP) ./src/* $(PKG_BUILD_DIR)/
#	$(call Build/Patch/Default)
#endef

TARGET_CFLAGS += $(FPIC)

define Build/Compile
endef

define Build/Configure
endef


define Build/InstallDev
	$(INSTALL_DIR) $(1)/usr/include
	$(CP) $(PKG_BUILD_DIR)/*.h $(1)/usr/include/
	$(INSTALL_DIR) $(1)/usr/lib
	$(CP) $(PKG_BUILD_DIR)/Binary/* $(1)/
endef

define Package/lib-dni-common-api/install
	$(CP) -rf $(PKG_BUILD_DIR)/Binary/* $(1)/
endef

$(eval $(call BuildPackage,lib-dni-common-api))
