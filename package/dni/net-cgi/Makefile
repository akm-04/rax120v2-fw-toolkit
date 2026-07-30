# 
# Copyright (C) 2009 OpenWrt.org
#

include $(TOPDIR)/rules.mk

PKG_NAME:=net-cgi
PKG_RELEASE:=1
PKG_BUILD_DIR := $(BUILD_DIR)/$(PKG_NAME)

WEB_PATH:=www

include $(INCLUDE_DIR)/package.mk

PKG_GIT_TREEISH:=c30d45077c1b01b621a45761020224c7c05a4a03

define Package/net-cgi
  SECTION:=utils
  CATEGORY:=DNI Packages
  DEPENDS:=libcurl +libiconv-full +datalib +libpthread +ntgrcrypt +d2 +dil +dniutil +libjson-c
  TITLE:=The mini_httpd CGI interface
endef


define Build/Prepare
	$(CP) $(GIT_HOME)/net-cgi.git/* $(PKG_BUILD_DIR)/
endef

define Build/Compile
endef

define Build/Configure
endef
define Package/net-cgi/install
	$(CP) -rf $(PKG_BUILD_DIR)/Binary/* $(1)/
endef

$(eval $(call BuildPackage,net-cgi))
