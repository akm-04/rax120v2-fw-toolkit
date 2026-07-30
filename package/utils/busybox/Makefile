#
# Copyright (C) 2006-2020 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

include $(TOPDIR)/rules.mk

PKG_NAME:=busybox
PKG_VERSION:=1.31.1
PKG_RELEASE:=9
PKG_FLAGS:=essential

PKG_BUILD_DEPENDS:=BUSYBOX_USE_LIBRPC:librpc BUSYBOX_CONFIG_PAM:libpam
PKG_BUILD_PARALLEL:=1
PKG_CHECK_FORMAT_SECURITY:=0

#Busybox use it's own PIE config flag and LDFLAGS are used with ld, not gcc.
PKG_ASLR_PIE:=0

PKG_LICENSE:=GPL-2.0
PKG_LICENSE_FILES:=LICENSE archival/libarchive/bz/LICENSE
PKG_CPE_ID:=cpe:/a:busybox:busybox

BUSYBOX_SYM=$(if $(CONFIG_BUSYBOX_CUSTOM),CONFIG,DEFAULT)
BUSYBOX_IF_ENABLED=$(if $(CONFIG_BUSYBOX_$(BUSYBOX_SYM)_$(1)),$(2))

ifneq ($(CONFIG_BUSYBOX_$(BUSYBOX_SYM)_FEATURE_SUID),)
  PKG_FILE_MODES:=/bin/busybox:root:root:4755
endif

include $(INCLUDE_DIR)/package.mk

ifeq ($(DUMP),)
  STAMP_CONFIGURED:=$(strip $(STAMP_CONFIGURED))_$(shell grep '^CONFIG_BUSYBOX_' $(TOPDIR)/.config | md5)
endif

# All files provided by busybox will serve as fallback alternatives by opkg.
# There should be no need to enumerate ALTERNATIVES entries here
define Package/busybox/Default
  SECTION:=base
  CATEGORY:=Base system
  MAINTAINER:=Felix Fietkau <nbd@nbd.name>
  TITLE:=Core utilities for embedded Linux
  URL:=http://busybox.net/
  DEPENDS:=+BUSYBOX_USE_LIBRPC:librpc +BUSYBOX_CONFIG_PAM:libpam +BUSYBOX_CONFIG_NTPD:jsonfilter +libcrypto +libopenssl
  USERID:=ntp=123:ntp=123
endef

define Package/busybox
  $(call Package/busybox/Default)
  CONFLICTS:=busybox-selinux
  VARIANT:=default
endef

define Package/busybox-selinux
  $(call Package/busybox/Default)
  TITLE += with SELinux support
  DEPENDS += +libselinux
  VARIANT:=selinux
  PROVIDES:=busybox
endef

define Package/busybox/description
 The Swiss Army Knife of embedded Linux.
 It slices, it dices, it makes Julian Fries.
endef

define Package/busybox/config
	source "$(SOURCE)/Config.in"
endef

ifdef CONFIG_BUSYBOX_CONFIG_FEATURE_SYSLOG
define Package/busybox/conffiles
/etc/syslog.conf
endef

Package/busybox-selinux/conffiles = $(Package/busybox/conffiles)
endif

# don't create a version string containing the actual timestamp
export KCONFIG_NOTIMESTAMP=1


ifndef CONFIG_USE_MUSL
LDLIBS:=m crypt
endif

LDLIBS += $(call BUSYBOX_IF_ENABLED,PAM,pam pam_misc pthread)
ifdef CONFIG_BUSYBOX_USE_LIBRPC
  LDLIBS += rpc
endif
ifeq ($(CONFIG_USE_GLIBC),y)
  LDLIBS += $(call BUSYBOX_IF_ENABLED,NSLOOKUP_OPENWRT,resolv)
endif

ifeq ($(BUILD_VARIANT),selinux)
  LDLIBS += selinux sepol
endif

TARGET_CFLAGS += -flto
TARGET_LDFLAGS += -flto=jobserver -fuse-linker-plugin

MAKE_VARS :=
MAKE_FLAGS += \
	EXTRA_CFLAGS="$(TARGET_CFLAGS) $(TARGET_CPPFLAGS)" \
	EXTRA_LDFLAGS="$(TARGET_LDFLAGS)" \
	LDLIBS="$(LDLIBS)" \
	LD="$(TARGET_CC)" \
	SKIP_STRIP=y
ifneq ($(findstring c,$(OPENWRT_VERBOSE)),)
  MAKE_FLAGS += V=1
endif

PKG_GIT_TREEISH:="20ceb8f8179183c275ecb29c4dacd69d900a17f4"
BUSYBOX_HOME:="busybox.git"

define Build/Prepare
	$(CP) $(GIT_HOME)/busybox.git/* $(PKG_BUILD_DIR)/
endef

define Build/Configure
	rm -f $(PKG_BUILD_DIR)/.config
	touch $(PKG_BUILD_DIR)/.config
ifeq ($(DEVICE_TYPE),nas)
	echo "CONFIG_HDPARM=y" >> $(PKG_BUILD_DIR)/.config
endif
ifeq ($(BUILD_VARIANT),selinux)
	cat $(TOPDIR)/$(SOURCE)/selinux.config >> $(PKG_BUILD_DIR)/.config
endif
	grep 'CONFIG_BUSYBOX_$(BUSYBOX_SYM)' $(TOPDIR)/.config | sed -e "s,\\(# \)\\?CONFIG_BUSYBOX_$(BUSYBOX_SYM)_\\(.*\\),\\1CONFIG_\\2,g" >> $(PKG_BUILD_DIR)/.config
	yes 'n' | $(MAKE) -C $(PKG_BUILD_DIR) $(MAKE_FLAGS) oldconfig
endef

define Build/Compile
	$(call Build/Compile/Default, \
		CONFIG_PREFIX="$(PKG_INSTALL_DIR)" \
		all install \
	)
endef

define Package/busybox/install
	$(INSTALL_DIR) $(1)/etc/init.d
	$(INSTALL_DIR) $(1)/usr/sbin
	$(CP) $(PKG_INSTALL_DIR)/* $(1)/
ifneq ($(CONFIG_BUSYBOX_$(BUSYBOX_SYM)_CROND),)
	$(INSTALL_BIN) ./files/cron $(1)/etc/init.d/cron
endif
	for tmp in $(init-y); do \
		$(INSTALL_BIN) $(PKG_BUILD_DIR)/dni-examples/$$$$tmp $(1)/etc/init.d/$$$$tmp; \
	done
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/dni-examples/dsyslog $(1)/usr/sbin/
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/dni-examples/dsyslog $(1)/etc/init.d/syslogd
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/dni-examples/cmd_cron $(1)/usr/sbin/
	$(INSTALL_BIN) ./files/cron $(1)/etc/init.d/
	$(INSTALL_DIR) $(1)/etc/bandcheck
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/dni-examples/band-check $(1)/etc/bandcheck/band-check

	-rm -rf $(1)/lib64
endef

Package/busybox-selinux/install = $(Package/busybox/install)

$(eval $(call BuildPackage,busybox))
