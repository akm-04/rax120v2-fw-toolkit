#sources of the HAL drivers
HAL_DRIVER_SOURCES = \
	$(HAL_TOP)/drivers/iofic/al_hal_iofic.c \
	$(HAL_TOP)/drivers/udma/al_hal_udma_main.c \
	$(HAL_TOP)/drivers/udma/al_hal_udma_config.c \
	$(HAL_TOP)/drivers/udma/al_hal_udma_iofic.c \
	$(HAL_TOP)/drivers/udma/al_hal_m2m_udma.c \
	$(HAL_TOP)/drivers/udma/al_hal_udma_debug.c \
	$(HAL_TOP)/drivers/udma/al_hal_msg_ipc.c	\
	$(HAL_TOP)/drivers/eth/al_hal_eth_main.c \
	$(HAL_TOP)/drivers/eth/al_hal_eth_kr.c \
	$(HAL_TOP)/drivers/ssm/al_hal_ssm.c \
	$(HAL_TOP)/drivers/ssm/al_hal_ssm_raid.c	\
	$(HAL_TOP)/drivers/ssm/al_hal_ssm_crypto.c \
	$(HAL_TOP)/drivers/ssm/al_hal_ssm_crc_memcpy.c \
	$(HAL_TOP)/drivers/serdes/al_hal_serdes.c \
	$(HAL_TOP)/drivers/pcie/al_hal_pcie.c \
	$(HAL_TOP)/drivers/pcie/al_hal_pcie_interrupts.c \
	$(HAL_TOP)/drivers/ddr/al_hal_ddr.c \
	$(HAL_TOP)/drivers/ddr/al_hal_ddr_init.c \
	$(HAL_TOP)/drivers/ddr/al_hal_ddr_pmu.c \
	$(HAL_TOP)/drivers/pbs/al_hal_muio_mux.c \
	$(HAL_TOP)/drivers/pbs/al_hal_spi.c \
	$(HAL_TOP)/drivers/pbs/al_hal_nand.c \
	$(HAL_TOP)/drivers/pbs/al_hal_nand_dma.c \
	$(HAL_TOP)/drivers/pbs/al_hal_bootstrap.c \
	$(HAL_TOP)/drivers/pbs/al_hal_gpio.c \
	$(HAL_TOP)/drivers/pbs/al_hal_sgpo.c \
	$(HAL_TOP)/drivers/pbs/al_hal_i2c.c \
	$(HAL_TOP)/drivers/pbs/al_hal_uart.c \
	$(HAL_TOP)/drivers/pbs/al_hal_addr_map.c \
	$(HAL_TOP)/drivers/pbs/al_hal_tdm.c \
	$(HAL_TOP)/drivers/ring/al_hal_pll.c \
	$(HAL_TOP)/drivers/sys_services/al_hal_timer.c \
	$(HAL_TOP)/drivers/sys_services/al_hal_thermal_sensor.c \
	$(HAL_TOP)/drivers/sys_services/al_hal_watchdog.c \
	$(HAL_TOP)/drivers/sys_services/al_hal_otp.c \
	$(HAL_TOP)/drivers/sys_fabric/al_hal_iommu.c \
	$(HAL_TOP)/drivers/sys_fabric/al_hal_ccu_pmu.c \
	$(HAL_TOP)/drivers/sys_fabric/al_hal_cpu_push_packet.c \
	$(HAL_TOP)/drivers/ring/al_hal_cmos.c \
	$(HAL_TOP)/drivers/udma_fast/al_hal_udma_fast.c \
	$(HAL_TOP)/drivers/io_fabric/al_hal_unit_adapter.c \

#sources of the init files compiled in the HAL itself
HAL_INIT_SOURCES_GENERIC = \
	$(HAL_TOP)/services/eth/al_init_eth_lm.c \
	$(HAL_TOP)/services/eth/al_init_eth_kr.c \
	$(HAL_TOP)/services/pcie/al_init_pcie.c \
	$(HAL_TOP)/services/pcie/al_init_pcie_debug.c \
	$(HAL_TOP)/services/sys_fabric/al_init_sys_fabric.c \
	$(HAL_TOP)/services/flash_contents/al_flash_contents.c \

HAL_INIT_SOURCES_ARM = \
	$(HAL_TOP)/services/monitor_mgmt/aarch32/monitor_mgmt.S \

HAL_INIT_SOURCES_AARCH64 = \
	$(HAL_TOP)/services/monitor_mgmt/aarch64/monitor_mgmt.S \

#include path that a HAL user needs
HAL_USER_INCLUDE_PATH = \
	-I$(HAL_TOP)/include/common \
	-I$(HAL_TOP)/include/io_fabric \
	-I$(HAL_TOP)/include/iofic \
	-I$(HAL_TOP)/include/udma\
	-I$(HAL_TOP)/include/ssm \
	-I$(HAL_TOP)/include/eth \
	-I$(HAL_TOP)/include/pbs \
	-I$(HAL_TOP)/include/ring \
	-I$(HAL_TOP)/include/sys_services \
	-I$(HAL_TOP)/include/serdes \
	-I$(HAL_TOP)/include/pcie \
	-I$(HAL_TOP)/include/sys_fabric \
	-I$(HAL_TOP)/include/ddr \
	-I$(HAL_TOP)/include/udma_fast \

#include path additioons for compiling the drivers
HAL_DRIVER_INCLUDE_PATH = \
	-I$(HAL_TOP)/drivers/ddr \
	-I$(HAL_TOP)/drivers/eth \
	-I$(HAL_TOP)/drivers/pbs \
	-I$(HAL_TOP)/drivers/pcie \
	-I$(HAL_TOP)/drivers/ring \
	-I$(HAL_TOP)/drivers/serdes/ \
	-I$(HAL_TOP)/drivers/ssm \
	-I$(HAL_TOP)/drivers/sys_fabric \
	-I$(HAL_TOP)/drivers/sys_services \

#include path additioons for using init functions
HAL_INIT_INCLUDE_PATH = \
	-I$(HAL_TOP)/services/gic/ \
	-I$(HAL_TOP)/services/eth/ \
	-I$(HAL_TOP)/services/flash_contents \
	-I$(HAL_TOP)/services/sys_fabric/ \
	-I$(HAL_TOP)/services/cpu_resume/ \
	-I$(HAL_TOP)/services/pcie/ \

#include path additions for using init functions - aarch32
HAL_INIT_INCLUDE_PATH_AARCH32 = \
	-I$(HAL_TOP)/services/monitor_mgmt/aarch32/ \

#include path additions for using init functions - aarch64
HAL_INIT_INCLUDE_PATH_AARCH64 = \
	-I$(HAL_TOP)/services/monitor_mgmt/aarch64/ \

HAL_PLATFORM_INCLUDE_PATH_ALPINE_V1 = \
	-I$(HAL_TOP)/platform/alpine_v1/include

HAL_PLATFORM_INCLUDE_PATH_ALPINE_V2 = \
	-I$(HAL_TOP)/platform/alpine_v2/include

HAL_PLATFORM_INCLUDE_PATH_ALPINE_HW29765235P0P512P1024P4X4P4X4 = \
	-I$(HAL_TOP)/platform/alpine_hw29765235p0p512p1024p4x4p4x4/include

