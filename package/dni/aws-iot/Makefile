#
# Copyright (C) 2007-2008 Delta Networks Inc.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=aws-iot
PKG_VERSION:=36.0
PKG_RELEASE:=36
PKG_BUILD_DEPENDS:=datalib

PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)

CONFIG_AWS_IOT_GIT_TREEISH:="14e5e602e8aedfddf667977763c0ba7c1e679aa4"
CONFIG_AWS_IOT_GIT_REPOSITORY:="aws-iot.git"

PKG_GIT_TREEISH:=$(CONFIG_AWS_IOT_GIT_TREEISH)

LOCAL_PKG_NAME_EXT:=aws-iot

include $(INCLUDE_DIR)/package.mk

define Package/aws-iot
  SECTION:=utils
  CATEGORY:=Utilities
  DEPENDS:=+datalib +libcurl +libjson-c
  TITLE:=The AWS IoT to Publish Topic
endef

EXTRA_LDFLAGS := $(EXTRA_CFLAGS) $(call qstrip,$(CONFIG_AWSIOT_EXTRA_FLAGS))

ifdef CONFIG_USE_MUSL
  TARGET_CFLAGS += -DCONFIG_USE_MUSL
endif


define Build/Prepare
	$(CP) $(GIT_HOME)/aws-iot.git/* $(PKG_BUILD_DIR)/
endef

define Build/Compile
endef

define Build/Configure
endef

define Package/aws-iot/install
	$(CP) -rf $(PKG_BUILD_DIR)/Binary/* $(1)/
endef

$(eval $(call BuildPackage,aws-iot))
