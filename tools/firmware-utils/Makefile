SRC_DIR = src
BIN_DIR = ../../bin/firmware-utils

CC ?= gcc
CFLAGS ?= -O2 -include endian.h -include byteswap.h -Wall
LDFLAGS ?=

# List of all binaries to be generated (used for cleaning)
BINARIES = addpattern asustrx trx motorola-bin dgfirmware mksenaofw trx2usr \
           ptgen airlink srec2bin mkmylofw mkcsysimg mkzynfw lzma2eva mkcasfw \
           mkfwimage mkfwimage2 imagetag add_header makeamitbin encode_crc \
           nand_ecc mkplanexfw mktplinkfw mktplinkfw2 tplink-safeloader pc1crypt \
           osbridge-crc wrt400n rax120 mkdniimg mktitanimg mkchkimg mkzcfw \
           spw303v zyxbcm trx2edips xorimage buffalo-enc buffalo-tag \
           buffalo-tftp mkwrgimg mkedimaximg mkbrncmdline mkbrnimg mkdapimg \
           mkcameofw seama oseama fix-u-media-header hcsmakeimage mkporayfw \
           mkhilinkfw mkdcs932 mkheader_gemtek mkrtn56uimg dgn3500sum \
           edimax_fw_header

# Macro to compile tools
# $(1) = source files, $(2) = extra libraries
define cc
	@echo "  CC      $(firstword $(1))"
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(BIN_DIR)/$(firstword $(1)) $(foreach src,$(1),$(SRC_DIR)/$(src).c) $(2)
endef

.PHONY: all install clean

all: install

install: $(BIN_DIR)
	@echo "Building firmware-utils tools..."
	$(call cc,addpattern)
	$(call cc,asustrx)
	$(call cc,trx)
	$(call cc,motorola-bin)
	$(call cc,dgfirmware)
	$(call cc,mksenaofw md5)
	$(call cc,trx2usr)
	$(call cc,ptgen)
	$(call cc,airlink)
	$(call cc,srec2bin)
	$(call cc,mkmylofw)
	$(call cc,mkcsysimg)
	$(call cc,mkzynfw)
	$(call cc,lzma2eva,-lz)
	$(call cc,mkcasfw)
	$(call cc,mkfwimage,-lz)
	$(call cc,mkfwimage2,-lz)
	$(call cc,imagetag imagetag_cmdline cyg_crc32)
	$(call cc,add_header)
	$(call cc,makeamitbin)
	$(call cc,encode_crc)
	$(call cc,nand_ecc)
	$(call cc,mkplanexfw sha1)
	$(call cc,mktplinkfw md5)
	$(call cc,mktplinkfw2 md5)
	$(call cc,tplink-safeloader md5, -Wall)
	$(call cc,pc1crypt)
	$(call cc,osbridge-crc)
	$(call cc,wrt400n cyg_crc32)
	$(call cc,rax120)
	$(call cc,mkdniimg)
	$(call cc,mktitanimg)
	$(call cc,mkchkimg)
	$(call cc,mkzcfw cyg_crc32)
	$(call cc,spw303v)
	$(call cc,zyxbcm)
	$(call cc,trx2edips)
	$(call cc,xorimage)
	$(call cc,buffalo-enc buffalo-lib, -Wall)
	$(call cc,buffalo-tag buffalo-lib, -Wall)
	$(call cc,buffalo-tftp buffalo-lib, -Wall)
	$(call cc,mkwrgimg md5, -Wall)
	$(call cc,mkedimaximg)
	$(call cc,mkbrncmdline)
	$(call cc,mkbrnimg)
	$(call cc,mkdapimg)
	$(call cc,mkcameofw, -Wall)
	$(call cc,seama md5)
	$(call cc,oseama md5, -Wall)
	$(call cc,fix-u-media-header cyg_crc32,-Wall)
	$(call cc,hcsmakeimage bcmalgo)
	$(call cc,mkporayfw, -Wall)
	$(call cc,mkhilinkfw, -lcrypto)
	$(call cc,mkdcs932, -Wall)
	$(call cc,mkheader_gemtek,-lz)
	$(call cc,mkrtn56uimg, -lz)
	$(call cc,dgn3500sum)
	$(call cc,edimax_fw_header, -Wall)

$(BIN_DIR):
	@mkdir -p $(BIN_DIR)

clean:
	@echo "Cleaning firmware-utils binaries..."
	@rm -f $(addprefix $(BIN_DIR)/, $(BINARIES))
