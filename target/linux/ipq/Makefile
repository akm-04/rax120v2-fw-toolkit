# Copyright (c) 2013 The Linux Foundation. All rights reserved.
#
include $(TOPDIR)/rules.mk

ARCH:=arm
BOARD:=ipq
BOARDNAME:=Qualcomm Atheros IPQ
FEATURES:=ubifs squashfs pcie usbgadget
CPU_TYPE:=cortex-a7
SUBTARGETS:=ipq806x ipq40xx ipq807x ipq807x_64 ipq60xx ipq60xx_64
MAINTAINER:=John Crispin <blogic@openwrt.org>

KERNELNAME=Image dtbs
LINUX_VERSION:=4.4.60

CUSTOMER:=dni
LINUX_TREEISH:=250701d289408be209e8612b83aa9e41865dffcc
#NETFILTER_TREEISH:=b728672d2cfddf8fbf914d85d212719b678620cd

# Kernel clone path and local repository reference can be done through menu config, else we we
# can hardcode it as below, this is just for a reference
CONFIG_KERNEL_GIT_CLONE_URI:=$(GIT_HOME)/qsdk-linux.git
CONFIG_KERNEL_GIT_BRANCH:=""
CONFIG_KERNEL_GIT_LOCAL_REPOSITORY:=""

include $(INCLUDE_DIR)/target.mk
DEFAULT_PACKAGES += \
	uboot-envtools kmod-leds-gpio kmod-gpio-button-hotplug kmod-button-hotplug \
	kmod-ata-core kmod-ata-ahci kmod-ata-ahci-platform kmod-usb2 kmod-usb3 \
	kmod-usb-dwc3-qcom kmod-usb-phy-qcom-dwc3 kmod-usb-dwc3-of-simple kmod-usb-f-qdss swconfig

$(eval $(call BuildTarget))
