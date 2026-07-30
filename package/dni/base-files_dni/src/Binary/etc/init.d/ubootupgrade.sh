#!/bin/sh

version=`devmem 0x0194D000`
ubootversion=`strings /dev/mtd15 |grep U-boot |awk '{print $3}'`
nosmmu=`/bin/config get uboot_nosmmu`
mkdir -p /tmp/cache/uboot
#6       DEVCFG           BCH           0x900000        0x80000 devcfg_nosmmu.mbn
#15      APPSBL           BCH           0xd80000       0x100000     openwrt-ipq807x-u-boot.mbn

upgrade_uboot()
{
	uboot=$1
	tar xvf /lib/uboot/$uboot.tar.gz -C /tmp/cache/uboot/
	tar xvf /lib/uboot/devcfg.tar.gz -C /tmp/cache/uboot/
	if [ "$?" = "0" ];then
		nandwrite -p /dev/mtd15 /tmp/cache/uboot/$uboot/openwrt-ipq807x-u-boot.mbn
		if [ "$uboot" = "ubootv16-crash" ]; then
			flash_erase /dev/mtd6 0 0
			nandwrite -p /dev/mtd6 /tmp/cache/uboot/devcfg.mbn
			/bin/config set uboot_nosmmu=1
			/bin/config commit
			#sleep 3
		fi
		echo "++++++Upgrade uboot successfully and will reboot now !!!!!!!!!!"	
		reboot -f
	else
		echo "Unzip the $1.tar.gz fail !!!"  > /dev/console
		return
	fi
}

upgrade_to_crashdump()
{
	echo "!!!!!!!!!!upgrade crashdump uboot!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"  > /dev/console
	#if [  "x$version" = "x0x200D0102" -a "x$ubootversion" = "xV1.2" ];then
	#	upgrade_uboot ubootv12-crash
	#elif [ "x$version" = "x0x200D0200" -a "x$ubootversion" = "xV1.6" ];then
	#	upgrade_uboot ubootv16-crash
	#elif [ "x$nosmmu" != "x1" -a "x$ubootversion" = "xV1.6-Crashdump_enable" ];then
	if [ "x$ubootversion" = "xV1.6" -o "x$ubootversion" = "xV1.6-Crashdump_enable" ]; then
		if [ "x$nosmmu" != "x1" ]; then
			tar xvf /lib/uboot/devcfg.tar.gz -C /tmp/cache/uboot/
			if [ -f /tmp/cache/uboot/devcfg.mbn ];then
				echo "++++++Upgrade devcfg.mbn successfully and will reboot now !!!!!!!!!!"
				flash_erase /dev/mtd6 0 0
				nandwrite -p /dev/mtd6 /tmp/cache/uboot/devcfg.mbn
				/bin/config set uboot_nosmmu=1
				/bin/config commit
				sleep 3
				reboot -f
			fi 
		fi
	fi
}

upgrade_to_normal()
{
	echo "!!!!!!!!!!upgrade normal uboot!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"  > /dev/console
	if [  "x$version" = "x0x200D0102" -a "x$ubootversion" = "xV1.2-crashdump-enable" ];then
		upgrade_uboot ubootv12
	elif [ "x$version" = "x0x200D0200" -a "x$ubootversion" = "xV1.6-Crashdump_enable" ];then
		upgrade_uboot ubootv16
	fi
}

case $1 in
	crash)
	upgrade_to_crashdump
	;;
	normal)
	upgrade_to_normal
	;;
esac
