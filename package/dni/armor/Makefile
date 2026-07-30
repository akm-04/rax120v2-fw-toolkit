include $(TOPDIR)/rules.mk
	
PKG_NAME:=armor
PKG_RELEASE:=1
			  
PKG_BUILD_DIR:=$(BUILD_DIR)/$(PKG_NAME)
			    
include $(INCLUDE_DIR)/package.mk

LOCAL_PKG_NAME_EXT:=armor

define Package/armor
	CATEGORY:=Bigfoot
	TITLE:=armor and bitdefender related files
	DEPENDS:= +libpthread
endef

					
define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Build/Compile
endef

define Package/armor/install
	$(CP) -rf $(PKG_BUILD_DIR)/Binary/* $(1)/
endef
							
#call func is to expand BuildPackage with "armor" as the param.
$(eval $(call BuildPackage,armor))
