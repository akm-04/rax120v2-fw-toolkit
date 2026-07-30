#
# (C) Copyright 2000-2006
# Wolfgang Denk, DENX Software Engineering, wd@denx.de.
#
# SPDX-License-Identifier:	GPL-2.0+
#

#ccflags-y += -DDEBUG

obj-y += checksum.o
obj-$(CONFIG_CMD_NET)  += arp.o
obj-$(CONFIG_CMD_NET)  += bootp.o
obj-$(CONFIG_CMD_CDP)  += cdp.o
obj-$(CONFIG_CMD_DNS)  += dns.o
obj-$(CONFIG_CMD_NET)  += eth.o
obj-$(CONFIG_CMD_LINK_LOCAL) += link_local.o
obj-$(CONFIG_CMD_NET)  += net.o
obj-$(CONFIG_CMD_NFS)  += nfs.o
obj-$(CONFIG_CMD_PING) += ping.o
obj-$(CONFIG_CMD_RARP) += rarp.o
obj-$(CONFIG_CMD_SNTP) += sntp.o
obj-$(CONFIG_CMD_NET)  += tftp.o
obj-$(CONFIG_SYS_NMRP)  += nmrp.o
#obj-$(CONFIG_SYS_UPDATE_DATA)  += update_data.o
obj-$(CONFIG_SYS_UPDATE_DATA)  += update_data_common.o
obj-$(CONFIG_HW29765235P0P512P1024P4X4P4X4)  += update_data_nand.o
obj-$(CONFIG_HW29765589P0P512P1024P4X4P8X8)  += update_data_nand.o
obj-$(CONFIG_HW29766094P0P256P512P2X2P2X2)   += update_data_nand.o
obj-$(CONFIG_HW29765960P0P4000P1000P4X4P4X4P4X4)  += update_data_mmc.o
obj-$(CONFIG_HW29766097P0P512P1024P2X2P2X2P2X2)  += update_data_nand.o
obj-$(CONFIG_HW29766193P0P256P512P2X2P2X2) += update_data_nand.o