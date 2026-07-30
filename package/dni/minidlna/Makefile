#
# Copyright (C) 2010-2014 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=minidlna
PKG_VERSION:=1.3.0
PKG_RELEASE:=1
PKG_GIT_DIR:=minidlna.git

#PKG_SOURCE:=$(PKG_NAME)-$(PKG_VERSION).tar.gz
#PKG_SOURCE_URL:=@SF/minidlna
#PKG_HASH:=47d9b06b4c48801a4c1112ec23d24782728b5495e95ec2195bbe5c81bc2d3c63

PKG_MAINTAINER:=
PKG_LICENSE:=GPL-2.0-or-later BSD-3-Clause
PKG_LICENSE_FILES:=COPYING LICENCE.miniupnpd

PKG_FIXUP:=autoreconf
PKG_INSTALL:=1
PKG_BUILD_PARALLEL:=1

include $(INCLUDE_DIR)/package.mk
include $(INCLUDE_DIR)/nls.mk

config-y					+= --with-model_name=$(CONFIG_MINIDLNA_MODELNAME)

define Package/minidlna
  SECTION:=multimedia
  CATEGORY:=Multimedia
  TITLE:=UPnP A/V & DLNA Media Server
  URL:=http://minidlna.sourceforge.net/
  DEPENDS:= +libpthread +libexif +libjpeg +libsqlite3 +libffmpeg \
  	+libid3tag +libflac +libvorbis +libavahi-client $(ICONV_DEPENDS) $(INTL_DEPENDS)
  USERID:=minidlna:minidlna
endef

define Package/minidlna/description
  MiniDLNA (aka ReadyDLNA) is server software with the aim of
  being fully compliant with DLNA/UPnP-AV clients.
endef

define Package/minidlna/conffiles
/etc/config/minidlna
endef

define Build/Prepare
	$(CP) $(GIT_HOME)/minidlna.git/* $(PKG_BUILD_DIR)/
endef

TARGET_CFLAGS += -D_FILE_OFFSET_BITS=64

CONFIGURE_ARGS += \
	--target=$(GNU_TARGET_NAME) \
	--host=$(GNU_HOST_NAME) \
	--build=$(GNU_BUILD_NAME) \
	--with-libiconv-prefix="$(ICONV_PREFIX)" \
	--with-libintl-prefix="$(INTL_PREFIX)" \
	--with-os-name="OpenWrt Linux" \
	--with-os-version="$(LINUX_VERSION)" \
	--with-os-url="https://openwrt.org/" \
	--with-db-path="/var/run/minidlna" \
	--with-log-path="/var/log" \
	--enable-tivo \
	--enable-netgear \

define Package/minidlna/install
	$(INSTALL_DIR) $(1)/usr/sbin
	$(INSTALL_BIN) $(PKG_INSTALL_DIR)/usr/sbin/minidlnad $(1)/usr/sbin/minidlna
	$(INSTALL_DIR) $(1)/sbin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/cmddlna  $(1)/sbin/
endef

$(eval $(call BuildPackage,minidlna))
