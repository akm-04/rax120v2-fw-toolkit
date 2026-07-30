CONFIG_QCA_SINGLE_IMG_TREEISH = 8d8b06f17e9b4a6f9b008ded56cbe104d64806a2

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

	cp nand-ipq807x-single.img uboot-hw29766097p0p512p1024p2x2p2x2p2x2.img
endef
