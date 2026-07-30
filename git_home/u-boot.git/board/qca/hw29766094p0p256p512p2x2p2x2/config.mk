CONFIG_QCA_SINGLE_IMG_TREEISH = 5af6d5f7e1cd60a61b8d68823efd1aa7c29a788a

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

	cp nand-ipq807x-single.img uboot-hw29766094p0p256p512p2x2p2x2.img
endef
