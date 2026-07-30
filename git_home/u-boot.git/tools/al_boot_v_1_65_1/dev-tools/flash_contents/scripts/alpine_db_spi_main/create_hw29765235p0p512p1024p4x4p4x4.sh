#!/bin/bash

set -e

script_dir=`dirname $0`
common_dir=`cd $script_dir/../common/; pwd`

# TOC offset
toc_offset=0x0

# Flash layout definition, not including the TOC
# Each entry is in the following format:
# "address|id|instance|name|image-file"
# - address - Flash object address
# - id - Flash object ID - the following IDs are supported:
#   * PRE_BOOT - Pre-Boot (Annapurna Labs binary - mandatory)
#   * BOOT_MODE - Boot mode (Customer defined - optional)
#   * UBOOT - U-Boot (Customer built - mandatory)
#   * UBOOT_SCRIPT - U-Boot extra init (Customer defined - optional)
#   * DT - Device Tree (Customer defined - mandatory)
#   * UBOOT_ENV - U-Boot environment (Customer defined - mandatory)
#   * UBOOT_ENV_RED - U-Boot redundant environment (Customer defined - optional)
#   * KERNEL - Linux kernel
#   * ROOT_FS - Linux root file system
# - instance - flash object instance number
#              relevant for multiple image support - 0 in case of single image
# - name - Flash object textual name (up to 8 characters)
# - file name - The flash object image file that represents the object
#               This is not mandatory for objects of the secondary instance
#               (in case of multiple image support)
#               If no image is provided, the object is padded with 0xff
# Entry with address only defines the end address for previous object
toc=(
	"0x010000|BOOT_MODE|0|boot-mod|boot_mode.fimg"
	"0x020000|PRE_BOOT|0|preboot|preboot.fimg"
	"0x060000|UBOOT_SCRIPT|0|u-boot-scr|u-boot-script.img"
	"0x070000|DT|0|dt|dt.img"
	"0x080000|UBOOT|0|uboot|u-boot.img"
	"0x100000|PRE_BOOT|1|preboot"
	"0x150000|UBOOT_SCRIPT|1|u-boot-scr"
	"0x160000|DT|1|dt"
	"0x170000|UBOOT|1|uboot"
	"0x1e0000|UBOOT_ENV|0|uboot-env"
	"0x200000|UBOOT_ENV_RED|0|uboot-red"
	"0x220000"
)

# Flash object definition
# Each entry below defines how each object image is to be generated
# Each entry is in the following format:
# "Image file|input file|ID|load_addr|exec_addr|major.minor.fix"
# - Image file - The output image file name - this is the input for the above
#                flash layout definition entries
# - input file - The object input file name
# - ID - The object ID, corresponsing to the IDs used in the above flash layout
#        definition
# - load_addr - Loading address for the object - mandatory for U-Boot
# - exec_addr - Execution address for the object - mandatory for U-Boot
# - major.minor.fix - Object version
images=(
	"boot_mode.fimg|boot_mode|BOOT_MODE|0|0|${VER_BOOT_MODE}"
	"u-boot.img|u-boot.bin|UBOOT|0x100000|0x100000|${VER_UBOOT}"
	"u-boot-script.img|uboot_script_init.script|UBOOT_SCRIPT|0|0|${VER_UBOOT_SCRIPT}"
	"dt.img|alpine_db.dtb|DT|0|0|${VER_DT}"
)

#list of required files which are not images
readyFiles=(
	"preboot.fimg"
)

source $common_dir/functions
source $common_dir/main
