#
# (C) Copyright 2006
# Wolfgang Denk, DENX Software Engineering, wd@denx.de.
#
# SPDX-License-Identifier:	GPL-2.0+
#

obj-$(CONFIG_DM_SPI_FLASH) += sf-uclass.o

ifdef CONFIG_SPL_BUILD
obj-$(CONFIG_SPL_SPI_LOAD)	+= spi_spl_load.o
obj-$(CONFIG_SPL_SPI_BOOT)	+= fsl_espi_spl.o
endif

# obj-$(CONFIG_SPI_FLASH) += sf_probe.o spi_flash.o sf_params.o sf.o
obj-$(CONFIG_HW29765235P0P512P1024P4X4P4X4) += sf_probe.o spi_flash_hw29765235p0p512p1024p4x4p4x4.o sf_params.o sf.o
obj-$(CONFIG_HW29765589P0P512P1024P4X4P8X8) += sf_probe.o spi_flash_hw29765589p0p512p1024p4x4p8x8.o sf_params.o sf.o
obj-$(CONFIG_HW29766094P0P256P512P2X2P2X2) += sf_probe.o spi_flash_hw29766094p0p256p512p2x2p2x2.o sf_params.o sf.o
obj-$(CONFIG_HW29765960P0P4000P1000P4X4P4X4P4X4) += sf_probe.o spi_flash_hw29765960p0p4000p1000p4x4p4x4p4x4.o sf_params.o sf.o
obj-$(CONFIG_HW29766097P0P512P1024P2X2P2X2P2X2) += sf_probe.o spi_flash_hw29766097p0p512p1024p2x2p2x2p2x2.o sf_params.o sf.o
obj-$(CONFIG_HW29766193P0P256P512P2X2P2X2) += sf_probe.o spi_flash_hw29766193p0p256p512p2x2p2x2.o sf_params_hw29766193p0p256p512p2x2p2x2.o sf.o
obj-$(CONFIG_AM33XX) += sf_probe.o spi_flash_am33xx.o sf_params.o sf.o
obj-$(CONFIG_SPI_FLASH_DATAFLASH) += sf_dataflash.o
obj-$(CONFIG_SPI_FLASH_MTD) += sf_mtd.o
obj-$(CONFIG_SPI_FLASH_SANDBOX) += sandbox.o
obj-$(CONFIG_SPI_NAND) += spi_nand.o
