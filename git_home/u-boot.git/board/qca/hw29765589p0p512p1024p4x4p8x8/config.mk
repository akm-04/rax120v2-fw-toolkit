CONFIG_QCA_SINGLE_IMG_TREEISH = 755eae8dbee6b517c15f478616c0db199ca9d641

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

	cp nand-ipq807x-single.img uboot-hw29765589p0p512p1024p4x4p8x8.img
endef
