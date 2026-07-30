CONFIG_QCA_SINGLE_IMG_TREEISH = 63fc0ed42d5f989eac83b54d88ff57300af09c17

export CONFIG_QCA_SINGLE_IMG_TREEISH

single_img_dep = u-boot

define BuildSingleImg
	cp u-boot openwrt-ipq807x-u-boot.elf

	board/"$(BOARDDIR)"/gen-single-img.sh --force-remove \
			--git-repo "$(CONFIG_QCA_SINGLE_IMG_GIT)" \
			--treeish $(CONFIG_QCA_SINGLE_IMG_TREEISH) \
			-w "qsdk-chipcode" \
			-b "32" \
			-o . \
			openwrt-ipq807x-u-boot.elf

	cp emmc-ipq807x_uboot-single.img uboot-hw29765960p0p4000p1000p4x4p4x4p4x4.img
endef
