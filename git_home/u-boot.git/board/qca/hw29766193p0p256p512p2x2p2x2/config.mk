CONFIG_QCA_SINGLE_IMG_TREEISH = 6956fe347c1d158ebcbfe836aa3f9565aba63e41

export CONFIG_QCA_SINGLE_IMG_TREEISH

single_img_dep = u-boot

define BuildSingleImg
	cp u-boot openwrt-ipq6018-u-boot.elf

	board/"$(BOARDDIR)"/gen-single-img.sh --force-remove \
			--git-repo "$(CONFIG_QCA_SINGLE_IMG_GIT)" \
			--treeish $(CONFIG_QCA_SINGLE_IMG_TREEISH) \
			-w "qsdk-chipcode" \
			-b "32" \
			-o . \
			openwrt-ipq6018-u-boot.elf

	cp nand-ipq6018-single.img uboot-hw29766193p0p256p512p2x2p2x2.img
endef
