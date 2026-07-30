/*
 * Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <common.h>
#include <asm/global_data.h>
#include <asm/io.h>
#include <asm/errno.h>
#include <environment.h>
#include <nand.h>
#include <asm/arch-qca-common/qca_common.h>
#include <asm/arch-qca-common/qpic_nand.h>
#include <asm/arch-qca-common/gpio.h>
#include <asm/arch-qca-common/uart.h>
#include <asm/arch-qca-common/smem.h>
#include <asm/arch-qca-common/scm.h>
#include <fdtdec.h>
#include <mmc.h>
#include <usb.h>
#include <linux/linkage.h>
#include <sdhci.h>

#define DLOAD_MAGIC_COOKIE	0x10
DECLARE_GLOBAL_DATA_PTR;

#define GCNT_PSHOLD             0x004AB000

#ifndef CONFIG_SDHCI_SUPPORT
qca_mmc mmc_host;
#else
struct sdhci_host mmc_host;
#endif


extern loff_t board_env_offset;
extern loff_t board_env_range;
extern loff_t board_env_size;

extern int ipq_spi_init(u16);
extern int ipq807x_edma_init(void *cfg);

#ifdef CONFIG_QCA_MMC
#ifdef CONFIG_SDHCI_SUPPORT
extern int board_mmc_env_init(struct sdhci_host mmc_host);
#else
extern int board_mmc_env_init(qca_mmc mmc_host);
#endif
#endif

const char *rsvd_node = "/reserved-memory";
const char *del_node[] = {"uboot",
			  "sbl",
			  NULL};
const add_node_t add_fdt_node[] = {{}};
#ifdef CONFIG_PCI_IPQ
static int pci_initialised;
#endif
static int aq_phy_initialised;
struct dumpinfo_t dumpinfo_n[] = {
	/* TZ stores the DDR physical address at which it stores the
	 * APSS regs, NSS IMEM copy and PMIC dump. We will have the TZ IMEM
	 * IMEM Addr at which the DDR physical address is stored as
	 * the start
	 *     --------------------
         *     |  DDR phy (start) | ----> ------------------------
         *     --------------------       | APSS regsave (8k)    |
         *                                ------------------------
         *                                |                      |
	 *                                |  NSS IMEM copy       |
         *                                |        (384k)        |
	 *                                |                      |
         *                                ------------------------
	 *                                |  PMIC dump (8k)      |
	 *                                ------------------------
	 */
	{ "EBICS0.BIN", 0x40000000, 0x10000000, 0 },
	{ "CODERAM.BIN", 0x00200000, 0x00028000, 0 },
	{ "DATARAM.BIN", 0x00290000, 0x00014000, 0 },
	{ "MSGRAM.BIN", 0x00060000, 0x00006000, 1 },
	{ "IMEM.BIN", 0x08600000, 0x00001000, 0 },
	{ "NSSIMEM.BIN", 0x08600658, 0x00060000, 0, 1, 0x2000 },
};
int dump_entries_n = ARRAY_SIZE(dumpinfo_n);

struct dumpinfo_t dumpinfo_s[] = {
	{ "EBICS_S0.BIN", 0x40000000, 0xAC00000, 0 },
	{ "EBICS_S1.BIN", CONFIG_TZ_END_ADDR, 0x10000000, 0 },
	{ "DATARAM.BIN", 0x00290000, 0x00014000, 0 },
	{ "MSGRAM.BIN", 0x00060000, 0x00006000, 1 },
	{ "IMEM.BIN", 0x08600000, 0x00001000, 0 },
	{ "NSSIMEM.BIN", 0x08600658, 0x00060000, 0, 1, 0x2000 },
};
int dump_entries_s = ARRAY_SIZE(dumpinfo_s);

void uart2_configure_mux(void)
{
	unsigned long cfg_rcgr;

	cfg_rcgr = readl(GCC_BLSP1_UART2_APPS_CFG_RCGR);
	/* Clear mode, src sel, src div */
	cfg_rcgr &= ~(GCC_UART_CFG_RCGR_MODE_MASK |
			GCC_UART_CFG_RCGR_SRCSEL_MASK |
			GCC_UART_CFG_RCGR_SRCDIV_MASK);

	cfg_rcgr |= ((UART2_RCGR_SRC_SEL << GCC_UART_CFG_RCGR_SRCSEL_SHIFT)
			& GCC_UART_CFG_RCGR_SRCSEL_MASK);

	cfg_rcgr |= ((UART2_RCGR_SRC_DIV << GCC_UART_CFG_RCGR_SRCDIV_SHIFT)
			& GCC_UART_CFG_RCGR_SRCDIV_MASK);

	cfg_rcgr |= ((UART2_RCGR_MODE << GCC_UART_CFG_RCGR_MODE_SHIFT)
			& GCC_UART_CFG_RCGR_MODE_MASK);

	writel(cfg_rcgr, GCC_BLSP1_UART2_APPS_CFG_RCGR);
}

void uart2_set_rate_mnd(unsigned int m,
			unsigned int n, unsigned int two_d)
{
	writel(m, GCC_BLSP1_UART2_APPS_M);
	writel(NOT_N_MINUS_M(n, m), GCC_BLSP1_UART2_APPS_N);
	writel(NOT_2D(two_d), GCC_BLSP1_UART2_APPS_D);
}

int uart2_trigger_update(void)
{
	unsigned long cmd_rcgr;
	int timeout = 0;

	cmd_rcgr = readl(GCC_BLSP1_UART2_APPS_CMD_RCGR);
	cmd_rcgr |= UART2_CMD_RCGR_UPDATE;
	writel(cmd_rcgr, GCC_BLSP1_UART2_APPS_CMD_RCGR);

	while (readl(GCC_BLSP1_UART2_APPS_CMD_RCGR) & UART2_CMD_RCGR_UPDATE) {
		if (timeout++ >= CLOCK_UPDATE_TIMEOUT_US) {
			printf("Timeout waiting for UART2 clock update\n");
			return -ETIMEDOUT;
		}
		udelay(1);
	}
	cmd_rcgr = readl(GCC_BLSP1_UART2_APPS_CMD_RCGR);
	return 0;
}

void uart2_toggle_clock(void)
{
	unsigned long cbcr_val;

	cbcr_val = readl(GCC_BLSP1_UART2_APPS_CBCR);
	cbcr_val |= UART2_CBCR_CLK_ENABLE;
	writel(cbcr_val, GCC_BLSP1_UART2_APPS_CBCR);
}

void uart2_clock_config(unsigned int m,
			unsigned int n, unsigned int two_d)
{
	uart2_configure_mux();
	uart2_set_rate_mnd(m, n, two_d);
	uart2_trigger_update();
	uart2_toggle_clock();
}

void qca_serial_init(struct ipq_serial_platdata *plat)
{
	int node, uart2_node;

	writel(1, GCC_BLSP1_UART1_APPS_CBCR);

	node = fdt_path_offset(gd->fdt_blob, "/serial@78B3000/serial_gpio");
	if (node < 0) {
		printf("Could not find serial_gpio node\n");
		return;
	}

	if (plat->port_id == 1) {
		uart2_node = fdt_path_offset(gd->fdt_blob, "uart2");
		if (uart2_node < 0) {
			printf("Could not find uart2 node\n");
			return;
		}
		node = fdt_subnode_offset(gd->fdt_blob,
				uart2_node, "serial_gpio");
		uart2_clock_config(plat->m_value, plat->n_value, plat->d_value);
		writel(1, GCC_BLSP1_UART2_APPS_CBCR);
	}
	qca_gpio_init(node);
}

unsigned long timer_read_counter(void)
{
	return 0;
}

void reset_crashdump(void)
{
	unsigned int ret = 0;
	qca_scm_sdi();
	ret = qca_scm_dload(CLEAR_MAGIC);
	if (ret)
		printf ("Error in reseting the Magic cookie\n");
	return;
}

void psci_sys_reset(void)
{
	__invoke_psci_fn_smc(0x84000009, 0, 0, 0);
}

void qti_scm_pshold(void)
{
	int ret;

	ret = scm_call(SCM_SVC_BOOT, SCM_CMD_TZ_PSHOLD, NULL, 0, NULL, 0);

	if (ret != 0)
		writel(0, GCNT_PSHOLD);
}

void reset_cpu(unsigned long a)
{
	reset_crashdump();
	if (is_scm_armv8()) {
		psci_sys_reset();
	} else {
		qti_scm_pshold();
	}
	while(1);
}

void emmc_clock_config(int mode)
{
	/* Enable root clock generator */
	writel(readl(GCC_SDCC1_APPS_CBCR)|0x1, GCC_SDCC1_APPS_CBCR);
	/* Add 10us delay for CLK_OFF to get cleared */
	udelay(10);

	if (mode == MMC_IDENTIFY_MODE) {
		/* XO - 400Khz*/
		writel(0x2017, GCC_SDCC1_APPS_CFG_RCGR);
		/* Delay for clock operation complete */
		udelay(10);
		writel(0x1, GCC_SDCC1_APPS_M);
		writel(0xFC, GCC_SDCC1_APPS_N);
		writel(0xFD, GCC_SDCC1_APPS_D);
		/* Delay for clock operation complete */
		udelay(10);

	}
	if (mode == MMC_DATA_TRANSFER_MODE) {
		/* PLL0 - 50Mhz */
		writel(0x40F, GCC_SDCC1_APPS_CFG_RCGR);
		/* Delay for clock operation complete */
		udelay(10);
		writel(0x1, GCC_SDCC1_APPS_M);
		writel(0xFC, GCC_SDCC1_APPS_N);
		writel(0xFD, GCC_SDCC1_APPS_D);
		/* Delay for clock operation complete */
		udelay(10);
	}
	if (mode == MMC_DATA_TRANSFER_SDHCI_MODE) {
		/* PLL0 - 192Mhz */
		writel(0x20B, GCC_SDCC1_APPS_CFG_RCGR);
		/* Delay for clock operation complete */
		udelay(10);
		writel(0x1, GCC_SDCC1_APPS_M);
		writel(0xFC, GCC_SDCC1_APPS_N);
		writel(0xFD, GCC_SDCC1_APPS_D);
		/* Delay for clock operation complete */
		udelay(10);
	}
	/* Update APPS_CMD_RCGR to reflect source selection */
	writel(readl(GCC_SDCC1_APPS_CMD_RCGR)|0x1, GCC_SDCC1_APPS_CMD_RCGR);
	/* Add 10us delay for clock update to complete */
	udelay(10);
}

void emmc_clock_disable(void)
{
	/* Clear divider */
	writel(0x0, GCC_SDCC1_MISC);
}

void board_mmc_deinit(void)
{
	emmc_clock_disable();
}

void emmc_clock_reset(void)
{
	writel(0x1, GCC_SDCC1_BCR);
	udelay(10);
	writel(0x0, GCC_SDCC1_BCR);
}

void emmc_sdhci_init(void)
{
	writel(readl(MSM_SDC1_MCI_HC_MODE) & (~0x1), MSM_SDC1_MCI_HC_MODE);
	writel(readl(MSM_SDC1_BASE) | (1 << 7), MSM_SDC1_BASE); //SW_RST
	udelay(10);
	writel(readl(MSM_SDC1_MCI_HC_MODE) | (0x1), MSM_SDC1_MCI_HC_MODE);
}

int get_aquantia_gpio(void)
{
	int aquantia_gpio = -1, node;

	node = fdt_path_offset(gd->fdt_blob, "/ess-switch");
	if (node >= 0)
		aquantia_gpio = fdtdec_get_uint(gd->fdt_blob, node, "aquantia_gpio", -1);
	else
		return node;

	return aquantia_gpio;
}

int get_napa_gpio(int napa_gpio[2])
{
	int napa_gpio_cnt = -1, node;
	int res = -1;

	node = fdt_path_offset(gd->fdt_blob, "/ess-switch");

	if (node >= 0) {
		napa_gpio_cnt = fdtdec_get_uint(gd->fdt_blob, node, "napa_gpio_cnt", -1);
		if (napa_gpio_cnt >= 1) {
			res = fdtdec_get_int_array(gd->fdt_blob, node, "napa_gpio",
								(u32 *)napa_gpio, napa_gpio_cnt);
			if (res >= 0)
				return napa_gpio_cnt;
		}
	}

	return res;
}

void aquantia_phy_reset_init(void)
{
	int aquantia_gpio = -1, node;
	unsigned int *aquantia_gpio_base;

	if (!aq_phy_initialised) {
		node = fdt_path_offset(gd->fdt_blob, "/ess-switch");
		if (node >= 0)
			aquantia_gpio = fdtdec_get_uint(gd->fdt_blob, node, "aquantia_gpio", -1);

		if (aquantia_gpio >=0) {
			aquantia_gpio_base = (unsigned int *)GPIO_CONFIG_ADDR(aquantia_gpio);
			writel(0x203, aquantia_gpio_base);
			mdelay(500);
			gpio_direction_output(aquantia_gpio, 0x0);
		}
		aq_phy_initialised = 1;
	}
}

void aquantia_phy_reset_init_done(void)
{
	int aquantia_gpio;

	aquantia_gpio = get_aquantia_gpio();
	if (aquantia_gpio >= 0) {
		gpio_set_value(aquantia_gpio, 0x1);
	}
}

void napa_phy_reset_init(void)
{
	int napa_gpio[2] = {0}, napa_gpio_cnt, i;
	unsigned int *napa_gpio_base;

	napa_gpio_cnt = get_napa_gpio(napa_gpio);
	if (napa_gpio_cnt >= 1) {
		for (i = 0; i < napa_gpio_cnt; i++) {
			if (napa_gpio[i] >=0) {
				napa_gpio_base = (unsigned int *)GPIO_CONFIG_ADDR(napa_gpio[i]);
				writel(0x203, napa_gpio_base);
				gpio_direction_output(napa_gpio[i], 0x0);
			}
		}
	}
}

void sfp_reset_init(void)
{
	int sfp_gpio = -1, node;
	unsigned int *sfp_gpio_base;

	node = fdt_path_offset(gd->fdt_blob, "/ess-switch");
	if (node >= 0)
		sfp_gpio = fdtdec_get_uint(gd->fdt_blob, node, "sfp_gpio", -1);

	if (sfp_gpio >=0) {
		sfp_gpio_base = (unsigned int *)GPIO_CONFIG_ADDR(sfp_gpio);
		writel(0x2C1, sfp_gpio_base);
	}
}

void napa_phy_reset_init_done(void)
{
	int napa_gpio[2] = {0}, napa_gpio_cnt, i;

	napa_gpio_cnt = get_napa_gpio(napa_gpio);
	if (napa_gpio_cnt >= 1) {
		for (i = 0; i < napa_gpio_cnt; i++)
			gpio_set_value(napa_gpio[i], 0x1);

	}
}

void eth_clock_enable(void)
{
	int tlmm_base = 0x1025000;

	/*
	 * ethernet clk rcgr block init -- start
	 * these clk init will be moved to sbl later
	 */

	writel(0x100 ,0x01868024);
	writel(0x1 ,0x01868020);
	writel(0x2 ,0x01868020);
	writel(0x100 ,0x0186802C);
	writel(0x1 ,0x01868028);
	writel(0x2 ,0x01868028);
	writel(0x100 ,0x01868034);
	writel(0x1 ,0x01868030);
	writel(0x2 ,0x01868030);
	writel(0x100 ,0x0186803C);
	writel(0x1 ,0x01868038);
	writel(0x2 ,0x01868038);
	writel(0x100 ,0x01868044);
	writel(0x1 ,0x01868040);
	writel(0x2 ,0x01868040);
	writel(0x100 ,0x0186804C);
	writel(0x1 ,0x01868048);
	writel(0x2 ,0x01868048);
	writel(0x100 ,0x01868054);
	writel(0x1 ,0x01868050);
	writel(0x2 ,0x01868050);
	writel(0x100 ,0x0186805C);
	writel(0x1 ,0x01868058);
	writel(0x2 ,0x01868058);
	writel(0x100 ,0x01868064);
	writel(0x1 ,0x01868060);
	writel(0x2 ,0x01868060);
	writel(0x100 ,0x0186806C);
	writel(0x1 ,0x01868068);
	writel(0x2 ,0x01868068);
	writel(0x100 ,0x01868074);
	writel(0x1 ,0x01868070);
	writel(0x2 ,0x01868070);
	writel(0x100 ,0x0186807C);
	writel(0x1 ,0x01868078);
	writel(0x2 ,0x01868078);
	writel(0x101 ,0x01868084);
	writel(0x1 ,0x01868080);
	writel(0x2 ,0x01868080);
	writel(0x100 ,0x0186808C);
	writel(0x1 ,0x01868088);
	writel(0x2 ,0x01868088);

	/*
	 * ethernet clk rcgr block init -- end
	 * these clk init will be moved to sbl later
	 */

	/* bring phy out of reset */
	writel(7, tlmm_base + 0x1f000);
	writel(7, tlmm_base + 0x20000);
	writel(0x203, tlmm_base);
	writel(0, tlmm_base + 0x4);
	aquantia_phy_reset_init();
	napa_phy_reset_init();
	sfp_reset_init();
	mdelay(500);
	writel(2, tlmm_base + 0x4);
	aquantia_phy_reset_init_done();
	napa_phy_reset_init_done();
	mdelay(500);
}

int board_eth_init(bd_t *bis)
{
	int ret=0;

	eth_clock_enable();
	ret = ipq807x_edma_init(NULL);

	if (ret != 0)
		printf("%s: ipq807x_edma_init failed : %d\n", __func__, ret);

	return ret;
}

int board_mmc_init(bd_t *bis)
{
	int node;
	int ret = 0;
	qca_smem_flash_info_t *sfi = &qca_smem_flash_info;

	node = fdt_path_offset(gd->fdt_blob, "mmc");
	if (node < 0) {
		printf("sdhci: Node Not found, skipping initialization\n");
		return -1;
	}

#ifndef CONFIG_SDHCI_SUPPORT
	mmc_host.base = MSM_SDC1_BASE;
	mmc_host.clk_mode = MMC_IDENTIFY_MODE;
	emmc_clock_config(mmc_host.clk_mode);

	ret = qca_mmc_init(bis, &mmc_host);
#else
	mmc_host.ioaddr = (void *)MSM_SDC1_SDHCI_BASE;
	mmc_host.voltages = MMC_VDD_165_195;
	mmc_host.version = SDHCI_SPEC_300;
	mmc_host.cfg.part_type = PART_TYPE_EFI;
	mmc_host.quirks = SDHCI_QUIRK_BROKEN_VOLTAGE;

	emmc_clock_disable();
	emmc_clock_reset();
	udelay(10);
	emmc_clock_config(MMC_DATA_TRANSFER_SDHCI_MODE);
	emmc_sdhci_init();

	if (add_sdhci(&mmc_host, 200000000, 400000)) {
		printf("add_sdhci fail!\n");
		return -1;
	}
#endif

	if (!ret && sfi->flash_type == SMEM_BOOT_MMC_FLASH) {
		ret = board_mmc_env_init(mmc_host);
	}

	return ret;
}


void board_nand_init(void)
{
#ifdef CONFIG_QCA_SPI
	int gpio_node;
#endif

	qpic_nand_init();

#ifdef CONFIG_QCA_SPI
	gpio_node = fdt_path_offset(gd->fdt_blob, "/spi/spi_gpio");
	if (gpio_node >= 0) {
		qca_gpio_init(gpio_node);
		ipq_spi_init(CONFIG_IPQ_SPI_NOR_INFO_IDX);
	}
#endif
}
#ifdef CONFIG_PCI_IPQ
static void pcie_clock_init(int id)
{

	/* Enable PCIE CLKS */
	if (id == 0) {
		writel(0x2, GCC_PCIE0_AUX_CMD_RCGR);
		writel(0x107, GCC_PCIE0_AXI_CFG_RCGR);
		writel(0x1, GCC_PCIE0_AXI_CMD_RCGR);
		mdelay(100);
		writel(0x2, GCC_PCIE0_AXI_CMD_RCGR);
		writel(0x20000001, GCC_PCIE0_AHB_CBCR);
		writel(0x4FF1, GCC_PCIE0_AXI_M_CBCR);
		writel(0x20004FF1, GCC_PCIE0_AXI_S_CBCR);
		writel(0x1, GCC_PCIE0_AUX_CBCR);
		writel(0x80004FF1, GCC_PCIE0_PIPE_CBCR);
		writel(0x2, GCC_PCIE1_AUX_CMD_RCGR);
		writel(0x107, GCC_PCIE1_AXI_CFG_RCGR);
		writel(0x1, GCC_PCIE1_AXI_CMD_RCGR);
		mdelay(100);
		writel(0x2, GCC_PCIE1_AXI_CMD_RCGR);
		writel(0x20000001, GCC_PCIE1_AHB_CBCR);
		writel(0x4FF1, GCC_PCIE1_AXI_M_CBCR);
		writel(0x20004FF1, GCC_PCIE1_AXI_S_CBCR);
		writel(0x1, GCC_PCIE1_AUX_CBCR);
		writel(0x80004FF1, GCC_PCIE1_PIPE_CBCR);
		pci_initialised = 1;
	}
}

static void pcie_v2_clock_init(int id)
{

	/* Enable PCIE CLKS */
	if (id == 0) {
		writel(0x2, GCC_PCIE0_V2_AUX_CMD_RCGR);
		writel(0x107, GCC_PCIE0_AXI_CFG_RCGR);
		writel(0x1, GCC_PCIE0_V2_AXI_CMD_RCGR);
		mdelay(100);
		writel(0x2, GCC_PCIE0_V2_AXI_CMD_RCGR);
		writel(0x20000001, GCC_PCIE0_AHB_CBCR);
		writel(0x4FF1, GCC_PCIE0_AXI_M_CBCR);
		writel(0x20004FF1, GCC_PCIE0_AXI_S_CBCR);
		writel(0x1, GCC_PCIE0_AUX_CBCR);
		writel(0x80004FF1, GCC_PCIE0_PIPE_CBCR);
		writel(0x1, GCC_PCIE0_AXI_S_BRIDGE_CBCR);
		writel(0x10F, GCC_PCIE0_RCHNG_CFG_RCGR);
		writel(0x3, GCC_PCIE0_RCHNG_CMD_RCGR);

		writel(0x2, GCC_PCIE1_V2_AUX_CMD_RCGR);
		writel(0x107, GCC_PCIE1_AXI_CFG_RCGR);
		writel(0x1, GCC_PCIE1_V2_AXI_CMD_RCGR);
		mdelay(100);
		writel(0x2, GCC_PCIE1_V2_AXI_CMD_RCGR);
		writel(0x20000001, GCC_PCIE1_AHB_CBCR);
		writel(0x4FF1, GCC_PCIE1_AXI_M_CBCR);
		writel(0x20004FF1, GCC_PCIE1_AXI_S_CBCR);
		writel(0x1, GCC_PCIE1_AUX_CBCR);
		writel(0x80004FF1, GCC_PCIE1_PIPE_CBCR);
	}
}

static void pcie_v2_clock_deinit(int id)
{

	if (id == 0) {
		writel(0x0, GCC_PCIE0_AUX_CMD_RCGR);
		writel(0x0, GCC_PCIE0_AXI_CFG_RCGR);
		writel(0x0, GCC_PCIE0_AXI_CMD_RCGR);
		mdelay(100);
		writel(0x0, GCC_SYS_NOC_PCIE0_AXI_CLK);
		writel(0x0, GCC_PCIE0_AHB_CBCR);
		writel(0x0, GCC_PCIE0_AXI_M_CBCR);
		writel(0x0, GCC_PCIE0_AXI_S_CBCR);
		writel(0x0, GCC_PCIE0_AUX_CBCR);
		writel(0x0, GCC_PCIE0_PIPE_CBCR);
		writel(0x0, GCC_PCIE0_AXI_S_BRIDGE_CBCR);
		writel(0x0, GCC_PCIE0_RCHNG_CFG_RCGR);
		writel(0x0, GCC_PCIE0_RCHNG_CMD_RCGR);
		writel(0x0, GCC_PCIE1_V2_AUX_CMD_RCGR);
		writel(0x0, GCC_PCIE1_AXI_CFG_RCGR);
		writel(0x0, GCC_PCIE1_V2_AXI_CMD_RCGR);
		mdelay(100);
		writel(0x0, GCC_SYS_NOC_PCIE1_AXI_CLK);
		writel(0x0, GCC_PCIE1_AHB_CBCR);
		writel(0x0, GCC_PCIE1_AXI_M_CBCR);
		writel(0x0, GCC_PCIE1_AXI_S_CBCR);
		writel(0x0, GCC_PCIE1_AUX_CBCR);
		writel(0x0, GCC_PCIE1_PIPE_CBCR);
	}
}

static void pcie_clock_deinit(int id)
{

	if (id == 0) {
		writel(0x0, GCC_PCIE0_V2_AUX_CMD_RCGR);
		writel(0x0, GCC_PCIE0_V2_AXI_CMD_RCGR);
		writel(0x0, GCC_PCIE0_AXI_CMD_RCGR);
		mdelay(100);
		writel(0x0, GCC_SYS_NOC_PCIE0_AXI_CLK);
		writel(0x0, GCC_PCIE0_AHB_CBCR);
		writel(0x0, GCC_PCIE0_AXI_M_CBCR);
		writel(0x0, GCC_PCIE0_AXI_S_CBCR);
		writel(0x0, GCC_PCIE0_AUX_CBCR);
		writel(0x0, GCC_PCIE0_PIPE_CBCR);
		writel(0x0, GCC_PCIE1_AUX_CMD_RCGR);
		writel(0x0, GCC_PCIE1_AXI_CFG_RCGR);
		writel(0x0, GCC_PCIE1_AXI_CMD_RCGR);
		mdelay(100);
		writel(0x0, GCC_SYS_NOC_PCIE1_AXI_CLK);
		writel(0x0, GCC_PCIE1_AHB_CBCR);
		writel(0x0, GCC_PCIE1_AXI_M_CBCR);
		writel(0x0, GCC_PCIE1_AXI_S_CBCR);
		writel(0x0, GCC_PCIE1_AUX_CBCR);
		writel(0x0, GCC_PCIE1_PIPE_CBCR);
	}
}

void board_pci_init(int id)
{
	int node, gpio_node;
	char name[16];
	uint32_t soc_ver_major, soc_ver_minor;

	snprintf(name, sizeof(name), "pci%d", id);
	node = fdt_path_offset(gd->fdt_blob, name);
	if (node < 0) {
		printf("Could not find PCI in device tree\n");
		return;
	}
	gpio_node = fdt_subnode_offset(gd->fdt_blob, node, "pci_gpio");
	get_soc_version(&soc_ver_major, &soc_ver_minor);
	if (gpio_node >= 0)
		qca_gpio_init(gpio_node);
	if(soc_ver_major == 2)
		pcie_v2_clock_init(id);
	else
		pcie_clock_init(id);
	return;
}

void board_pci_deinit()
{
	int node, gpio_node, i, err;
	char name[16];
	struct fdt_resource parf;
	struct fdt_resource pci_phy;
	uint32_t soc_ver_major, soc_ver_minor;

	get_soc_version(&soc_ver_major, &soc_ver_minor);
	for (i = 0; i < PCI_MAX_DEVICES; i++) {
		snprintf(name, sizeof(name), "pci%d", i);
		node = fdt_path_offset(gd->fdt_blob, name);
		if (node < 0) {
			printf("Could not find PCI in device tree\n");
			return;
		}
		err = fdt_get_named_resource(gd->fdt_blob, node, "reg", "reg-names", "parf",
				&parf);
		writel(0x0, parf.start + 0x358);
		writel(0x1, parf.start + 0x40);
		err = fdt_get_named_resource(gd->fdt_blob, node, "reg", "reg-names", "pci_phy",
				     &pci_phy);
		if (err < 0) {

			if(soc_ver_major == 1) {
				err = fdt_get_named_resource(gd->fdt_blob, node, "reg", "reg-names", "pci_phy_gen2",
					     &pci_phy);
				if (err < 0)
					return;
			} else if(soc_ver_major == 2) {
				err = fdt_get_named_resource(gd->fdt_blob, node, "reg", "reg-names", "pci_phy_gen3",
					     &pci_phy);
				if (err < 0)
					return;
			} else {
				return;
			}
		}
		writel(0x1, pci_phy.start + 800);
		writel(0x0, pci_phy.start + 804);
		gpio_node = fdt_subnode_offset(gd->fdt_blob, node, "pci_gpio");
		if (gpio_node >= 0)
			qca_gpio_deinit(gpio_node);

	}

	if (soc_ver_major == 2)
		pcie_v2_clock_deinit(0);
	else
		pcie_clock_deinit(0);

	return ;
}
#endif

#ifdef CONFIG_USB_XHCI_IPQ
void board_usb_deinit(int id)
{
	void __iomem *boot_clk_ctl, *usb_bcr, *qusb2_phy_bcr;
	void __iomem *usb_phy_bcr, *usb_gen_cfg, *usb_guctl, *phy_base;

	if (id == 0) {
		boot_clk_ctl = (u32 *)GCC_USB_0_BOOT_CLOCK_CTL;
		usb_bcr = (u32 *)GCC_USB0_BCR;
		qusb2_phy_bcr = (u32 *)GCC_QUSB2_0_PHY_BCR;
		usb_phy_bcr = (u32 *)GCC_USB0_PHY_BCR;
		usb_gen_cfg = (u32 *)USB30_1_GENERAL_CFG;
		usb_guctl = (u32 *)USB30_1_GUCTL;
		phy_base = (u32 *)USB30_PHY_1_QUSB2PHY_BASE;
	}
	else if (id == 1) {
		boot_clk_ctl = (u32 *)GCC_USB_1_BOOT_CLOCK_CTL;
		usb_bcr = (u32 *)GCC_USB1_BCR;
		qusb2_phy_bcr = (u32 *)GCC_QUSB2_1_PHY_BCR;
		usb_phy_bcr = (u32 *)GCC_USB1_PHY_BCR;
		usb_gen_cfg = (u32 *)USB30_2_GENERAL_CFG;
		usb_guctl = (u32 *)USB30_2_GUCTL;
		phy_base = (u32 *)USB30_PHY_2_QUSB2PHY_BASE;
	}
	else {
		return;
	}
	//Enable USB2 PHY Power down
	setbits_le32(phy_base+0xB4, 0x1);

	if (id == 0) {
		writel(0x8000, GCC_USB0_PHY_CFG_AHB_CBCR);
		writel(0xcff0, GCC_USB0_MASTER_CBCR);
		writel(0, GCC_SYS_NOC_USB0_AXI_CBCR);
		writel(0, GCC_SNOC_BUS_TIMEOUT2_AHB_CBCR);
		writel(0, GCC_USB0_SLEEP_CBCR);
		writel(0, GCC_USB0_MOCK_UTMI_CBCR);
		writel(0, GCC_USB0_AUX_CBCR);
	}
	else if (id == 1) {
		writel(0x8000, GCC_USB1_PHY_CFG_AHB_CBCR);
		writel(0xcff0, GCC_USB1_MASTER_CBCR);
		writel(0, GCC_SYS_NOC_USB1_AXI_CBCR);
		writel(0, GCC_SNOC_BUS_TIMEOUT3_AHB_CBCR);
		writel(0, GCC_USB1_SLEEP_CBCR);
		writel(0, GCC_USB1_MOCK_UTMI_CBCR);
		writel(0, GCC_USB1_AUX_CBCR);
	}

	//GCC_QUSB2_0_PHY_BCR
	setbits_le32(qusb2_phy_bcr, 0x1);
	mdelay(10);
	clrbits_le32(qusb2_phy_bcr, 0x1);

	//GCC_USB0_PHY_BCR
	setbits_le32(usb_phy_bcr, 0x1);
	mdelay(10);
	clrbits_le32(usb_phy_bcr, 0x1);

	//GCC Reset USB0 BCR
	setbits_le32(usb_bcr, 0x1);
	mdelay(10);
	clrbits_le32(usb_bcr, 0x1);
}

static void usb_clock_init(int id)
{
	if (id == 0) {
		writel(0x222000, GCC_USB0_GDSCR);
		writel(0, GCC_SYS_NOC_USB0_AXI_CBCR);
		writel(0, GCC_SNOC_BUS_TIMEOUT2_AHB_CBCR);
		writel(0x10b, GCC_USB0_MASTER_CFG_RCGR);
		writel(0x1, GCC_USB0_MASTER_CMD_RCGR);
		writel(1, GCC_SYS_NOC_USB0_AXI_CBCR);
		writel(0xcff1, GCC_USB0_MASTER_CBCR);
		writel(1, GCC_SNOC_BUS_TIMEOUT2_AHB_CBCR);
		writel(1, GCC_USB0_SLEEP_CBCR);
		writel(0x210b, GCC_USB0_MOCK_UTMI_CFG_RCGR);
		writel(0x1, GCC_USB0_MOCK_UTMI_M);
		writel(0xf7, GCC_USB0_MOCK_UTMI_N);
		writel(0xf6, GCC_USB0_MOCK_UTMI_D);
		writel(0x3, GCC_USB0_MOCK_UTMI_CMD_RCGR);
		writel(1, GCC_USB0_MOCK_UTMI_CBCR);
		writel(0x8001, GCC_USB0_PHY_CFG_AHB_CBCR);
		writel(1, GCC_USB0_AUX_CBCR);
		writel(1, GCC_USB0_PIPE_CBCR);
	}
	else if (id == 1) {
		writel(0x222000, GCC_USB1_GDSCR);
		writel(0, GCC_SYS_NOC_USB1_AXI_CBCR);
		writel(0, GCC_SNOC_BUS_TIMEOUT3_AHB_CBCR);
		writel(0x10b, GCC_USB1_MASTER_CFG_RCGR);
		writel(0x1, GCC_USB1_MASTER_CMD_RCGR);
		writel(1, GCC_SYS_NOC_USB1_AXI_CBCR);
		writel(0xcff1, GCC_USB1_MASTER_CBCR);
		writel(1, GCC_SNOC_BUS_TIMEOUT3_AHB_CBCR);
		writel(1, GCC_USB1_SLEEP_CBCR);
		writel(0x210b, GCC_USB1_MOCK_UTMI_CFG_RCGR);
		writel(0x1, GCC_USB1_MOCK_UTMI_M);
		writel(0xf7, GCC_USB1_MOCK_UTMI_N);
		writel(0xf6, GCC_USB1_MOCK_UTMI_D);
		writel(0x3, GCC_USB1_MOCK_UTMI_CMD_RCGR);
		writel(1, GCC_USB1_MOCK_UTMI_CBCR);
		writel(0x8001, GCC_USB1_PHY_CFG_AHB_CBCR);
		writel(1, GCC_USB1_AUX_CBCR);
		writel(1, GCC_USB1_PIPE_CBCR);
	}
}

static void usb_init_ssphy(int index)
{
	void __iomem *phybase;

	if (index == 0) {
		phybase = (u32 *)USB0_SSPHY_BASE;
	}
	else if (index == 1) {
		phybase = (u32 *)USB1_SSPHY_BASE;
	} else
		return;

	out_8( phybase + USB3_PHY_POWER_DOWN_CONTROL,0x1);
	out_8(phybase + QSERDES_COM_SYSCLK_EN_SEL,0x1a);
	out_8(phybase + QSERDES_COM_BIAS_EN_CLKBUFLR_EN,0x08);
	out_8(phybase + QSERDES_COM_CLK_SELECT,0x30);
	out_8(phybase + QSERDES_COM_BG_TRIM,0x0f);
	out_8(phybase + QSERDES_RX_UCDR_FASTLOCK_FO_GAIN,0x0b);
	out_8(phybase + QSERDES_COM_SVS_MODE_CLK_SEL,0x01);
	out_8(phybase + QSERDES_COM_HSCLK_SEL,0x00);
	out_8(phybase + QSERDES_COM_CMN_CONFIG,0x06);
	out_8(phybase + QSERDES_COM_PLL_IVCO,0x0f);
	out_8(phybase + QSERDES_COM_SYS_CLK_CTRL,0x06);
	out_8(phybase + QSERDES_COM_DEC_START_MODE0,0x82);
	out_8(phybase + QSERDES_COM_DIV_FRAC_START1_MODE0,0x55);
	out_8(phybase + QSERDES_COM_DIV_FRAC_START2_MODE0,0x55);
	out_8(phybase + QSERDES_COM_DIV_FRAC_START3_MODE0,0x03);
	out_8(phybase + QSERDES_COM_CP_CTRL_MODE0,0x0b);
	out_8(phybase + QSERDES_COM_PLL_RCTRL_MODE0,0x16);
	out_8(phybase + QSERDES_COM_PLL_CCTRL_MODE0,0x28);
	out_8(phybase + QSERDES_COM_INTEGLOOP_GAIN0_MODE0,0x80);
	out_8(phybase + QSERDES_COM_LOCK_CMP1_MODE0,0x15);
	out_8(phybase + QSERDES_COM_LOCK_CMP2_MODE0,0x34);
	out_8(phybase + QSERDES_COM_LOCK_CMP3_MODE0,0x00);
	out_8(phybase + QSERDES_COM_CORE_CLK_EN,0x00);
	out_8(phybase + QSERDES_COM_LOCK_CMP_CFG,0x00);
	out_8(phybase + QSERDES_COM_VCO_TUNE_MAP,0x00);
	out_8(phybase + QSERDES_COM_BG_TIMER,0x0a);
	out_8(phybase + QSERDES_COM_SSC_EN_CENTER,0x01);
	out_8(phybase + QSERDES_COM_SSC_PER1,0x31);
	out_8(phybase + QSERDES_COM_SSC_PER2,0x01);
	out_8(phybase + QSERDES_COM_SSC_ADJ_PER1,0x00);
	out_8(phybase + QSERDES_COM_SSC_ADJ_PER2,0x00);
	out_8(phybase + QSERDES_COM_SSC_STEP_SIZE1,0xde);
	out_8(phybase + QSERDES_COM_SSC_STEP_SIZE2,0x07);
	out_8(phybase + QSERDES_RX_UCDR_SO_GAIN,0x06);
	out_8(phybase + QSERDES_RX_RX_EQU_ADAPTOR_CNTRL2,0x02);
	out_8(phybase + QSERDES_RX_RX_EQU_ADAPTOR_CNTRL3,0x6c);
	out_8(phybase + QSERDES_RX_RX_EQU_ADAPTOR_CNTRL3,0x4c);
	out_8(phybase + QSERDES_RX_RX_EQU_ADAPTOR_CNTRL4,0xb8);
	out_8(phybase + QSERDES_RX_RX_EQ_OFFSET_ADAPTOR_CNTRL,0x77);
	out_8(phybase + QSERDES_RX_RX_OFFSET_ADAPTOR_CNTRL2,0x80);
	out_8(phybase + QSERDES_RX_SIGDET_CNTRL,0x03);
	out_8(phybase + QSERDES_RX_SIGDET_DEGLITCH_CNTRL,0x16);
	out_8(phybase + QSERDES_RX_SIGDET_ENABLES,0x0c);
	out_8(phybase + QSERDES_TX_HIGHZ_TRANSCEIVEREN_BIAS_D,0x45);
	out_8(phybase + QSERDES_TX_RCV_DETECT_LVL_2,0x12);
	out_8(phybase + QSERDES_TX_LANE_MODE,0x06);
	out_8(phybase + PCS_TXDEEMPH_M6DB_V0,0x15);
	out_8(phybase + PCS_TXDEEMPH_M3P5DB_V0,0x0e);
	out_8(phybase + PCS_FLL_CNTRL2,0x83);
	out_8(phybase + PCS_FLL_CNTRL1,0x02);
	out_8(phybase + PCS_FLL_CNT_VAL_L,0x09);
	out_8(phybase + PCS_FLL_CNT_VAL_H_TOL,0xa2);
	out_8(phybase + PCS_FLL_MAN_CODE,0x85);
	out_8(phybase + PCS_LOCK_DETECT_CONFIG1,0xd1);
	out_8(phybase + PCS_LOCK_DETECT_CONFIG2,0x1f);
	out_8(phybase + PCS_LOCK_DETECT_CONFIG3,0x47);
	out_8(phybase + PCS_POWER_STATE_CONFIG2,0x1b);
	out_8(phybase + PCS_RXEQTRAINING_WAIT_TIME,0x75);
	out_8(phybase + PCS_RXEQTRAINING_RUN_TIME,0x13);
	out_8(phybase + PCS_LFPS_TX_ECSTART_EQTLOCK,0x86);
	out_8(phybase + PCS_PWRUP_RESET_DLY_TIME_AUXCLK,0x04);
	out_8(phybase + PCS_TSYNC_RSYNC_TIME,0x44);
	out_8(phybase + PCS_RCVR_DTCT_DLY_P1U2_L,0xe7);
	out_8(phybase + PCS_RCVR_DTCT_DLY_P1U2_H,0x03);
	out_8(phybase + PCS_RCVR_DTCT_DLY_U3_L,0x40);
	out_8(phybase + PCS_RCVR_DTCT_DLY_U3_H,0x00);
	out_8(phybase + PCS_RX_SIGDET_LVL,0x88);
	out_8(phybase + USB3_PCS_TXDEEMPH_M6DB_V0,0x17);
	out_8(phybase + USB3_PCS_TXDEEMPH_M3P5DB_V0,0x0f);
	out_8(phybase + QSERDES_RX_SIGDET_ENABLES,0x0);
	out_8(phybase + USB3_PHY_START_CONTROL,0x03);
	out_8(phybase + USB3_PHY_SW_RESET,0x00);
}

static void usb_init_phy(int index)
{
	void __iomem *boot_clk_ctl, *usb_bcr, *qusb2_phy_bcr;
	void __iomem *usb_phy_bcr, *usb3_phy_bcr, *usb_gen_cfg, *usb_guctl, *phy_base;

	if (index == 0) {
		boot_clk_ctl = (u32 *)GCC_USB_0_BOOT_CLOCK_CTL;
		usb_bcr = (u32 *)GCC_USB0_BCR;
		qusb2_phy_bcr = (u32 *)GCC_QUSB2_0_PHY_BCR;
		usb_phy_bcr = (u32 *)GCC_USB0_PHY_BCR;
		usb3_phy_bcr = (u32 *)GCC_USB3PHY_0_PHY_BCR;
		usb_gen_cfg = (u32 *)USB30_1_GENERAL_CFG;
		usb_guctl = (u32 *)USB30_1_GUCTL;
		phy_base = (u32 *)USB30_PHY_1_QUSB2PHY_BASE;
	}
	else if (index == 1) {
		boot_clk_ctl = (u32 *)GCC_USB_1_BOOT_CLOCK_CTL;
		usb_bcr = (u32 *)GCC_USB1_BCR;
		qusb2_phy_bcr = (u32 *)GCC_QUSB2_1_PHY_BCR;
		usb_phy_bcr = (u32 *)GCC_USB1_PHY_BCR;
		usb3_phy_bcr = (u32 *)GCC_USB3PHY_1_PHY_BCR;
		usb_gen_cfg = (u32 *)USB30_2_GENERAL_CFG;
		usb_guctl = (u32 *)USB30_2_GUCTL;
		phy_base = (u32 *)USB30_PHY_2_QUSB2PHY_BASE;
	}
	else {
		return;
	}
	//2. Enable SS Ref Clock
	setbits_le32(GCC_USB_SS_REF_CLK_EN, 0x1);

	//3. Disable USB Boot Clock
	clrbits_le32(boot_clk_ctl, 0x0);

	//4. GCC Reset USB0 BCR
	setbits_le32(usb_bcr, 0x1);

	//5. Delay 100us
	mdelay(10);

	//6. GCC Reset USB0 BCR
	clrbits_le32(usb_bcr, 0x1);
	//7. GCC_QUSB2_0_PHY_BCR
	setbits_le32(qusb2_phy_bcr, 0x1);

	//8. GCC_USB0_PHY_BCR
	setbits_le32(usb_phy_bcr, 0x1);
	setbits_le32(usb3_phy_bcr, 0x1);

	//9. Delay 100us
	mdelay(10);

	//10. GCC_USB0_PHY_BCR
	clrbits_le32(usb3_phy_bcr, 0x1);
	clrbits_le32(usb_phy_bcr, 0x1);

	//11. GCC_QUSB2_0_PHY_BCR
	clrbits_le32(qusb2_phy_bcr, 0x1);

	//12. Delay 100us
	mdelay(10);

	//20. Config user control register
	writel(0x0c80c010, usb_guctl);

	//21. Enable USB2 PHY Power down
	setbits_le32(phy_base+0xB4, 0x1);

	//22. PHY Config Sequence
	out_8(phy_base+0x80, 0xF8);
	out_8(phy_base+0x84, 0x83);
	out_8(phy_base+0x88, 0x83);
	out_8(phy_base+0x8C, 0xC0);
	out_8(phy_base+0x9C, 0x14);
	out_8(phy_base+0x08, 0x30);
	out_8(phy_base+0x0C, 0x79);
	out_8(phy_base+0x10, 0x21);
	out_8(phy_base+0x90, 0x00);
	out_8(phy_base+0x18, 0x00);
	out_8(phy_base+0x1C, 0x9F);
	out_8(phy_base+0x04, 0x80);

	//23. Disable USB2 PHY Power down
	clrbits_le32(phy_base+0xB4, 0x1);

	usb_init_ssphy(index);
}

int ipq_board_usb_init(void)
{
	int i;

	for (i=0; i<CONFIG_USB_MAX_CONTROLLER_COUNT; i++) {
		usb_clock_init(i);
		usb_init_phy(i);
	}
	return 0;
}
#endif

void ipq_fdt_fixup_socinfo(void *blob)
{
	uint32_t cpu_type;
	uint32_t soc_version, soc_version_major, soc_version_minor;
	int nodeoff, ret;

	nodeoff = fdt_path_offset(blob, "/");

	if (nodeoff < 0) {
		printf("ipq: fdt fixup cannot find root node\n");
		return;
	}

	ret = ipq_smem_get_socinfo_cpu_type(&cpu_type);
	if (!ret) {
		ret = fdt_setprop(blob, nodeoff, "cpu_type",
				  &cpu_type, sizeof(cpu_type));
		if (ret)
			printf("%s: cannot set cpu type %d\n", __func__, ret);
	} else {
		printf("%s: cannot get socinfo\n", __func__);
	}

	ret = ipq_smem_get_socinfo_version((uint32_t *)&soc_version);
	if (!ret) {
		soc_version_major = SOCINFO_VERSION_MAJOR(soc_version);
		soc_version_minor = SOCINFO_VERSION_MINOR(soc_version);

		ret = fdt_setprop(blob, nodeoff, "soc_version_major",
				  &soc_version_major,
				  sizeof(soc_version_major));
		if (ret)
			printf("%s: cannot set soc_version_major %d\n",
			       __func__, soc_version_major);

		ret = fdt_setprop(blob, nodeoff, "soc_version_minor",
				  &soc_version_minor,
				  sizeof(soc_version_minor));
		if (ret)
			printf("%s: cannot set soc_version_minor %d\n",
			       __func__, soc_version_minor);
	} else {
		printf("%s: cannot get soc version\n", __func__);
	}
}

void ipq_fdt_fixup_usb_device_mode(void *blob)
{
	int nodeoff, ret, node;
	const char *usb_dr_mode = "peripheral"; /* Supported mode */
	const char *usb_max_speed = "high-speed";/* Supported speed */
	const char *usb_node[] = {"/soc/usb3@8A00000/dwc3@8A00000"};
	const char *usb_cfg;

	usb_cfg = getenv("usb_mode");
	if (!usb_cfg)
		return;

	if (strcmp(usb_cfg, usb_dr_mode)) {
		printf("fixup_usb: usb_mode can be either 'peripheral' or not set\n");
		return;
	}

	for (node = 0; node < ARRAY_SIZE(usb_node); node++) {
		nodeoff = fdt_path_offset(blob, usb_node[node]);
		if (nodeoff < 0) {
			printf("fixup_usb: unable to find node '%s'\n",
			       usb_node[node]);
			return;
		}
		ret = fdt_setprop(blob, nodeoff, "dr_mode",
				  usb_dr_mode,
				  (strlen(usb_dr_mode) + 1));
		if (ret)
			printf("fixup_usb: 'dr_mode' cannot be set");

		/* if mode is peripheral restricting to high-speed */
		ret = fdt_setprop(blob, nodeoff, "maximum-speed",
				  usb_max_speed,
				  (strlen(usb_max_speed) + 1));
		if (ret)
			printf("fixup_usb: 'maximum-speed' cannot be set");
	}
}

void fdt_fixup_auto_restart(void *blob)
{
	int nodeoff, ret;
	const char *node = "/soc/q6v5_wcss@CD00000";
	const char *paniconwcssfatal;

	paniconwcssfatal = getenv("paniconwcssfatal");

	if (!paniconwcssfatal)
		return;

	if (strncmp(paniconwcssfatal, "1", sizeof("1"))) {
		printf("fixup_auto_restart: invalid variable 'paniconwcssfatal'");
	} else {
		nodeoff = fdt_path_offset(blob, node);
		if (nodeoff < 0) {
			printf("fixup_auto_restart: unable to find node '%s'\n", node);
			return;
		}
		ret = fdt_delprop(blob, nodeoff, "qca,auto-restart");

		if (ret)
			printf("fixup_auto_restart: cannot delete property");
	}
	return;
}

#define TCSR_CPR	0x0193d008 /* TCSR_TZ_WONCE_2 */
#define BM(lsb, msb)            ((BIT(msb) - BIT(lsb)) + BIT(msb))

void fdt_fixup_cpr(void *blob)
{
	int node, subnode, phandle;
	int i, ret;
	uint64_t opp_hz[] = {1017600000, 1382400000, 1651200000,
			     1843200000, 1920000000, 2208000000};
	uint32_t opp_microvolt[] = {840000, 904000, 944000,
				    984000, 992000, 1064000};

	char *compatible[] = {"qcom,cpr4-ipq807x-apss-regulator",
			      "qcom,cpr3-ipq807x-npu-regulator",
			      "qcom,ipq807x-apm"};

	uint32_t tcsr_cpr = readl(TCSR_CPR);

	tcsr_cpr = (tcsr_cpr >> 8) & BM(0, 1);

	if (tcsr_cpr != 1)
		return;

	node = fdt_path_offset(blob,
				"/soc/qcom,spmi@200f000/pmic@1/regulators/s3");
	if (node < 0)
		return;

	phandle = fdt_get_phandle(blob, node);
	if (phandle <= 0)
		return;

	/* Set cpu-supply for all 4 cores */
	node = fdt_path_offset(blob, "/cpus");
	if (node < 0)
		return;

	subnode = fdt_first_subnode(blob, node);
	if (subnode < 0)
		return;

	ret = fdt_setprop_cell(blob, subnode, "cpu0-supply", phandle);
	if (ret)
		return;

	for (i = 1; i <= 3; i++) {
		subnode = fdt_next_subnode(blob, subnode);
		if (subnode < 0)
			return;

		ret = fdt_setprop_cell(blob, subnode, "cpu-supply", phandle);
		if (ret)
			return;
	}

	/* Set operating point */
	node = fdt_path_offset(blob, "/cpus/opp_table0");
	if (node < 0)
		return;

	subnode = fdt_first_subnode(blob, node);
	if (subnode < 0)
		return;

	ret = fdt_setprop_cell(blob, subnode,
			       "opp-microvolt", opp_microvolt[0]);
	if (ret)
		return;

	ret = fdt_setprop_u64(blob, subnode, "opp-hz", opp_hz[0]);
	if (ret)
		return;

	for (i = 1; i < ARRAY_SIZE(opp_microvolt); i++) {
		subnode = fdt_next_subnode(blob, subnode);
		if (subnode < 0)
			return;

		ret = fdt_setprop_cell(blob, subnode,
				       "opp-microvolt", opp_microvolt[i]);
		if (ret)
			return;

		ret = fdt_setprop_u64(blob, subnode, "opp-hz", opp_hz[i]);
		if (ret)
			return;
	}

	/* Delete opp06 subnode */
	subnode = fdt_next_subnode(blob, subnode);
	if (subnode < 0)
		return;

	ret = fdt_del_node(blob, subnode);
	if (ret)
		return;

	node = fdt_path_offset(blob,
				"/soc/qcom,spmi@200f000/pmic@1/regulators/s4");
	if (node < 0)
		return;

	phandle = fdt_get_phandle(blob, node);
	if (phandle <= 0)
		return;

	/* Delete npu-supply and mx-supply */
	node = fdt_path_offset(blob, "/soc/nss@40000000");
	if (node < 0)
		return;

	ret = fdt_delprop(blob, node, "npu-supply");
	if (node < 0)
		return;

	ret = fdt_delprop(blob, node, "mx-supply");
	if (node < 0)
		return;

	/* Disable cpr, apu */
	for (i = 0; i < ARRAY_SIZE(compatible); i++) {
		node = fdt_node_offset_by_compatible(blob, 0, compatible[i]);
		if (node < 0)
			return;
		ret = fdt_setprop_string(blob, node, "status", "disabled");
	}

	return;
}

void fdt_low_memory_fixup(void *blob)
{
	int node;
	int len;
	u64 *reg;
	char *wcnss_node = "/reserved-memory/wcnss@4b000000";
	char *wifi_dump_node = "/reserved-memory/wifi_dump@50500000";
	char *tzapp_node = "/reserved-memory/tzapp@4a400000";
	unsigned int wcnss_size = 0x03700000;
	unsigned int wifi_dump_size = 0x200000;
	char *mem_mode = getenv("low_mem_mode");
	int ret;

	if (!mem_mode)
		return;

	printf("Configuring kernel for low memory mode...\n");

	node = fdt_path_offset(blob, wcnss_node);
	if (node >= 0) {
		reg = (u64 *)fdt_getprop(blob, node, "reg", &len);
		if (reg != NULL)
			reg[1] = cpu_to_fdt64(wcnss_size);
	} else {
		printf("Node \"%s\" not found\n", wcnss_node);
	}

	node = fdt_path_offset(blob, wifi_dump_node);
	if (node >= 0) {
		reg = (u64 *)fdt_getprop(blob, node, "reg", &len);
		if (reg != NULL)
			reg[1] = cpu_to_fdt64(wifi_dump_size);
	} else {
		printf("Node \"%s\" not found\n", wifi_dump_node);
	}

	node = fdt_path_offset(blob, tzapp_node);
	if (node >= 0) {
		ret = fdt_del_node(blob, node);
		if (ret)
			printf("Cannot delete \"%s\" node\n", tzapp_node);

	} else {
		printf("Node \"%s\" not found\n", tzapp_node);
	}
}

void fdt_fixup_set_dload_warm_reset(void *blob)
{
	int nodeoff, ret, node;
	const char *dload_node = {"/soc/qca,scm_restart_reason"};
	uint32_t setval = 1; 

	nodeoff = fdt_path_offset(blob, dload_node);
	if (nodeoff < 0) { 
		printf("fixup_set_dload: unable to find node '%s'\n",
            dload_node);
		return;
    }    
	ret = fdt_setprop_u32(blob, nodeoff, "dload_status", setval);
	if (ret)
		printf("fixup_set_dload: 'dload_status' not set");

	ret = fdt_setprop_u32(blob, nodeoff, "dload_warm_reset", setval);
	if (ret)
		printf("fixup_set_dload: 'dload_warm_reset' not set");
}

void fdt_fixup_set_qce_fixed_key(void *blob)
{
	int node_off, ret, node;
	const char *qce_node = {"/soc/crypto@8e3a000"};

	node_off = fdt_path_offset(blob, qce_node);
	if (node_off < 0) {
		printf("qce_crypto: unable to find node '%s'\n",
			qce_node);
		return;
	}

	ret = fdt_setprop_u32(blob, node_off, "qce,use_fixed_hw_key", 1);
	if (ret)
		printf("qce_crypto: 'qce,use_fixed_hw_key' property no set");

}

void set_flash_secondary_type(qca_smem_flash_info_t *smem)
{
	return;
};

void enable_caches(void)
{
	qca_smem_flash_info_t *sfi = &qca_smem_flash_info;

	smem_get_boot_flash(&sfi->flash_type,
						&sfi->flash_index,
						&sfi->flash_chip_select,
						&sfi->flash_block_size,
						&sfi->flash_density);
	icache_enable();
	/*Skips dcache_enable during JTAG recovery */
	if (sfi->flash_type)
		dcache_enable();
}

void disable_caches(void)
{
	icache_disable();
	dcache_disable();
}

/*
 * To determine the spi flash addr is in 3 byte
 * or 4 byte.
 */
unsigned int get_smem_spi_addr_len(void)
{
	int ret;
	uint32_t spi_flash_addr_len;

	ret = smem_read_alloc_entry(SMEM_SPI_FLASH_ADDR_LEN,
					&spi_flash_addr_len, sizeof(uint32_t));
	if (ret != 0) {
		printf("SPI: using 3 byte address mode as default\n");
		spi_flash_addr_len = SPI_DEFAULT_ADDR_LEN;
	}

	return spi_flash_addr_len;
}

int apps_iscrashed(void)
{
	u32 *dmagic = (u32 *)0x193D100;

	if (*dmagic == DLOAD_MAGIC_COOKIE)
		return 1;

	return 0;
}

/*
 * Routine to get GPIO INPUT pin or set GPIO OUTPUT pin.
 *
 * Modified from gmac_handle_gpio() of drivers/net/nss/mii_gpio.c
 *
 * @param is_get     1: get INPUT pin, 0: set OUTPUT pin
 * @param pin        GPIO pin to get/set
 * @param write_val  value to write in set operation
 * @return           pin value in get; 0 in set
 */
#define GPIO_IN   0
#define GPIO_OUT  1
#define GPIO_SET  0
#define GPIO_GET  1
static inline uint32_t ipq40xx_handle_gpio(uint32_t is_get, uint32_t pin,
                                           uint32_t write_val)
{
    uint32_t addr = GPIO_IN_OUT_ADDR(pin);
    uint32_t val = readl(addr);

    if (is_get)
        return val & (1<<GPIO_IN);

    val &= (~(1 << GPIO_OUT));
    if (write_val)
        val |= (1 << GPIO_OUT);
    writel(val, addr);

    return 0;
}


/* Record LED status. Used by flush_led_status() */
u16 led_status;

/* forward reference */
void flush_led_status(u16 new_led_status, short led_num);

#define LSB(bits)              (bits & 0x1)

/*
 * Turn on/off LED.
 *
 * It assumes all LEDs are Active LOW. Call flush_led_stusta().
 *
 * arguments:
 *     led_mask: bit value == 1 means the LED is switched on/off
 *
 * global variables:
 *     led_status: Record LED status. Every bit represents an LED.
 *
 * macros:
 *     LED_NUM: number of LEDs
 */
#define led_on(led_mask)       do {                    \
               led_status &= ~(led_mask);              \
               flush_led_status(led_status, LED_NUM);  \
} while(0)

#define led_off(led_mask)      do {                    \
               led_status |= led_mask;                 \
               flush_led_status(led_status, LED_NUM);  \
} while(0)

/*
 * Flush all LED ON/OFF status through SIPO (Serial-In Parallel-Out) shift
 * register controller.
 *
 * arguments:
 *     new_led_status: integer with every bit representing an LED
 *
 *     led_num: number of LEDs to determine when all LEDs are flushed
 */
void flush_led_status(u16 new_led_status, short led_num)
{
	for (; led_num > 0; led_num--) {
		/* Negative egde of LED_CLK */
		gpio_set_value(LED_CLK_GPIO,0);
		udelay(LED_CLK_DELAY_USEC);

		if (LSB(new_led_status)) {
			gpio_set_value(LED_DATA_GPIO,1);
		} else {
			gpio_set_value(LED_DATA_GPIO,0);
		}

		/* Positive egde of LED_CLK */
		gpio_set_value(LED_CLK_GPIO,1);
		udelay(LED_CLK_DELAY_USEC);

		new_led_status = (new_led_status >> 1);
	}
	return;
}

int misc_init_r(void)
{

struct qca_gpio_config gmac_gpio_config = {0};

gmac_gpio_config.gpio = LED_CLK_GPIO;
gmac_gpio_config.func = 0;
gmac_gpio_config.out = GPIO_OUT_LOW;
gmac_gpio_config.pull = GPIO_PULL_UP;
gmac_gpio_config.drvstr = LED_CURRENT;
gmac_gpio_config.oe = GPIO_OE_ENABLE ;
gmac_gpio_config.vm = 0;
gmac_gpio_config.od_en = 0;
gmac_gpio_config.pu_res = 0;

gpio_tlmm_config(&gmac_gpio_config);
gmac_gpio_config.gpio = LED_DATA_GPIO;
gpio_tlmm_config(&gmac_gpio_config);
gmac_gpio_config.gpio = LED_CLR_GPIO;
gpio_tlmm_config(&gmac_gpio_config);
gpio_set_value(LED_CLR_GPIO,1);

gmac_gpio_config.gpio = LED_WHITE;
gpio_tlmm_config(&gmac_gpio_config);
gmac_gpio_config.gpio = LED_GREEN;
gpio_tlmm_config(&gmac_gpio_config);
gmac_gpio_config.gpio = LED_RED;
gpio_tlmm_config(&gmac_gpio_config);
gmac_gpio_config.gpio = LED_BLUE;
gpio_tlmm_config(&gmac_gpio_config);

led_off(0xffff);
gpio_set_value(LED_WHITE,1);
gpio_set_value(LED_GREEN,1);
gpio_set_value(LED_RED,1);
gpio_set_value(LED_BLUE,1);

//led_on(1 << PWR_LED);

gmac_gpio_config.gpio = POWER_LED_WHITE;
gpio_tlmm_config(&gmac_gpio_config);

gpio_set_value(POWER_LED_WHITE,0);

gmac_gpio_config.gpio = MALIBU_RESET;
gpio_tlmm_config(&gmac_gpio_config);
gpio_set_value(MALIBU_RESET,0);
gpio_set_value(MALIBU_RESET,1);

    return 0;
}

#ifdef CONFIG_DISPLAY_BOARDINFO
int checkboard(void)
{
    printf("U-boot dni1 V0.5 for DNI HW ID: 29765960; eMMC flash 4000MB; RAM 1024MB .\n");
	printf("developed based on 'qsdk-ipq807x.ilq.9.0-spf.9.0.ES' \n");
    return 0;
}
#endif

/*return value 0: not pressed, 1: pressed*/
int board_reset_button_is_press(void)
{
	if (ipq40xx_handle_gpio(GPIO_GET, RESET_BUTTON, 0))
		return 0;
	else
		return 1;
}

/*return value 0: not pressed, 1: pressed*/
int board_wps_button_is_press(void)
{
	if (ipq40xx_handle_gpio(GPIO_GET, WPS_BUTTON, 0))
		return 0;
	else
		return 1;
}

/*ledstat 0:on; 1:off*/
void board_power_led(int ledstat)
{
	if(ledstat)
		gpio_set_value(POWER_LED_WHITE,1);
	else
		gpio_set_value(POWER_LED_WHITE,0);
}

/*ledstat 0:on; 1:off*/
void board_test_led(int ledstat)
{
}

void do_test_led(cmd_tbl_t *cmdtp, int flag, int argc,char * const argv[])
{
	struct qca_gpio_config gmac_gpio_config = {0};

	gmac_gpio_config.gpio = LED_CLK_GPIO;
	gmac_gpio_config.func = 0;
	gmac_gpio_config.out = GPIO_OUT_LOW;
	gmac_gpio_config.pull = GPIO_PULL_UP;
	gmac_gpio_config.drvstr = LED_CURRENT;
	gmac_gpio_config.oe = GPIO_OE_ENABLE ;
	gmac_gpio_config.vm = 0;
	gmac_gpio_config.od_en = 0;
	gmac_gpio_config.pu_res = 0;

	int pin , val ;

	if (argc <2){
//		led_off(0xffff);
		gpio_set_value(POWER_LED_WHITE,1);
		gpio_set_value(LED_WHITE,1);
		gpio_set_value(LED_GREEN,1);
		gpio_set_value(LED_RED,1);
		gpio_set_value(LED_BLUE,1);
		printf("turn off all LEDs\n");
		return ;
	}else if (argc == 3){
		val = simple_strtol(argv[2], (char **)NULL, 10);
		pin = simple_strtol(argv[1], (char **)NULL, 16);
	}else{
		printf("Usage:\n%s\n", cmdtp->usage);
		return ;
	}

	if(val){
		if (pin == 1){
			gpio_set_value(LED_WHITE,0);
			printf("turn on white LED\n");
		}if (pin == 2){
			gpio_set_value(LED_GREEN,0);
			printf("turn on green LED\n");
		}if (pin == 3){
			gpio_set_value(LED_RED,0);
			printf("turn on red LED\n");
		}if (pin == 4){
			gpio_set_value(LED_BLUE,0);
			printf("turn on blue LED\n");
		}
	}else{
        if (pin == 1){
            gpio_set_value(LED_WHITE,1);
			printf("turn off white LED\n");
        }if (pin == 2){
            gpio_set_value(LED_GREEN,1);
			printf("turn off green LED\n");
        }if (pin == 3){
            gpio_set_value(LED_RED,1);
			printf("turn off red LED\n");
        }if (pin == 4){
            gpio_set_value(LED_BLUE,1);
			printf("turn off blue LED\n");
        }
	}
}

U_BOOT_CMD(
	test_led, 3, 0, do_test_led,
	"test led on/off",
	"\n"
	"test_led <pin> <on/off> , 1 is on , 0 is off"
);

/* for NAND, 'addr' must align to flash page (usually, 0x800) */
int board_flash_read (char *src, ulong addr, ulong cnt)
{
	char cmdbuf[256];

	snprintf(cmdbuf, sizeof(cmdbuf), "nand read 0x%lX 0x%lX 0x%lX", (ulong)src, addr, cnt);
	debug("cmd : %s\n", cmdbuf);
	if (run_command(cmdbuf, 0) != CMD_RET_SUCCESS) {
		printf ("%s failed : %s\n", __FUNCTION__, cmdbuf);
		return 1;
	}
	return 0;
}

/* for NAND, 'addr_first' must align to flash sector (usually, 0x20000) */
int board_flash_sect_erase (ulong addr_first, ulong addr_last)
{
	char cmdbuf[256];

	snprintf(cmdbuf, sizeof(cmdbuf), "nand erase 0x%lX 0x%lX", addr_first, (addr_last - addr_first + 1));
	printf("cmd : %s\n", cmdbuf);
	if (run_command(cmdbuf, 0) != CMD_RET_SUCCESS) {
		printf ("%s failed : %s\n", __FUNCTION__, cmdbuf);
		return 1;
	}
	return 0;
}

/* for NAND, 'addr' & 'cnt' must both align to flash page (usually, 0x800) */
int board_flash_write (char *src, ulong addr, ulong cnt)
{
	char cmdbuf[256];

	snprintf(cmdbuf, sizeof(cmdbuf), "nand write 0x%lX 0x%lX 0x%lX", (ulong)src, addr, cnt);
	printf("cmd : %s\n", cmdbuf);
	if (run_command(cmdbuf, 0) != CMD_RET_SUCCESS) {
		printf ("%s failed : %s\n", __FUNCTION__, cmdbuf);
		return 1;
	}
	return 0;
}

/* 'off' & 'max_len' must both align to flash sector (usually, 0x20000) */
int board_flash_enough (int data_size, ulong off, ulong max_len, char* data_name)
{
	int dev = nand_curr_device;
	nand_info_t * nand = &nand_info[dev];
	ulong block_size = nand->erasesize;
	ulong len = 0;
	ulong len_incl_bad = 0;

	while (len < data_size) {
		if (off >= nand->size)
			goto not_enough;

		if (!nand_block_isbad (nand, (loff_t) off))
			len += block_size;

		len_incl_bad += block_size;
		off += block_size;
	}

	if (max_len >= len_incl_bad)
		return 1;

not_enough:
	if (data_name)
		printf("** FAIL !! no enough space for %s.\n", data_name);
	return 0;
}

/* 'off' must align to flash sector (usually, 0x20000) */
int board_flash_sect_isbad (ulong off)
{
	int dev = nand_curr_device;
	nand_info_t * nand = &nand_info[dev];

	if (nand_block_isbad(nand, (loff_t) off)) {
		printf("Device %d offset 0x%lX is bad.\n", dev, off);
		return 1;
	}
	return 0;
}

#if defined(NETGEAR_BOARD_ID_SUPPORT)
/*
 * item_name_want could be "device" to get Model Id, "version" to get Version
 * or "hd_id" to get Hardware ID.
 */
void board_get_image_info(ulong fw_image_addr, char *item_name_want, char *buf)
{
	char image_header[HEADER_LEN+1];
	char item_name[HEADER_LEN+1];
	char *item_value;
	char *parsing_string;

	memset(image_header, 0, HEADER_LEN);
	memcpy(image_header, fw_image_addr, HEADER_LEN);
	image_header[HEADER_LEN]='\0';

	parsing_string = strtok(image_header, "\n");
	while (parsing_string != NULL) {
		char *colon_p;
		memset(item_name, 0, sizeof(item_name));
		colon_p = strchr(parsing_string, ':');
		if (colon_p == NULL) {
			break;
		}
		strncpy(item_name, parsing_string, (int)(colon_p - parsing_string));

		if (strcmp(item_name, item_name_want) == 0) {
			item_value = strstr(parsing_string, ":") + 1;

			memcpy(buf, item_value, strlen(item_value));
		}

		parsing_string = strtok(NULL, "\n");
	}
}

int board_match_image_hw_id (ulong fw_image_addr)
{
	char board_hw_id[BOARD_HW_ID_LENGTH + 1];
	char image_hw_id[BOARD_HW_ID_LENGTH + 1];

	/*get hardward id from board */
	memset(board_hw_id, 0, sizeof(board_hw_id));
	get_board_data(BOARD_HW_ID_OFFSET, BOARD_HW_ID_LENGTH, (u8 *)board_hw_id);
	printf("HW ID on board: %s\n", board_hw_id);

	/*get hardward id from image */
	memset(image_hw_id, 0, sizeof(image_hw_id));
	board_get_image_info(fw_image_addr, "hd_id", image_hw_id);
	printf("HW ID on image: %s\n", image_hw_id);

	/* Only check first 29 chars hw29765960p0p4000p1000p4x4p4x4p4x4 */
	if (memcmp(board_hw_id, image_hw_id, BOARD_HW_ID_REAL_LENGTH) != 0) {
        printf("Firmware Image HW ID do not match Board HW ID\n");
        return 0;
	}
	printf("Firmware Image HW ID matched Board HW ID\n\n");
	return 1;
}

int board_match_image_model_id (ulong fw_image_addr)
{
	char board_model_id[BOARD_MODEL_ID_LENGTH + 1];
	char image_model_id[BOARD_MODEL_ID_LENGTH + 1];

	/*get hardward id from board */
	memset(board_model_id, 0, sizeof(board_model_id));
	get_board_data(BOARD_MODEL_ID_OFFSET, BOARD_MODEL_ID_LENGTH, (u8 *)board_model_id);
	printf("MODEL ID on board: %s\n", board_model_id);

	/*get hardward id from image */
	memset(image_model_id, 0, sizeof(image_model_id));
	board_get_image_info(fw_image_addr, "device", image_model_id);
	printf("MODEL ID on image: %s\n", image_model_id);

	if (memcmp(board_model_id, image_model_id, BOARD_MODEL_ID_LENGTH) != 0) {
        printf("Firmware Image MODEL ID do not match Board model ID\n");
        return 0;
	}
	printf("Firmware Image MODEL ID matched Board model ID\n\n");
	return 1;
}

void board_update_image_model_id (ulong fw_image_addr)
{
	char board_model_id[BOARD_MODEL_ID_LENGTH + 1];
	char image_model_id[BOARD_MODEL_ID_LENGTH + 1];

	/*get model id from board */
	memset(board_model_id, 0, sizeof(board_model_id));
	get_board_data(BOARD_MODEL_ID_OFFSET, BOARD_MODEL_ID_LENGTH, board_model_id);
	printf("Original board MODEL ID: %s\n", board_model_id);

	/*get model id from image */
	memset(image_model_id, 0, sizeof(image_model_id));
	board_get_image_info(fw_image_addr, "device", image_model_id);
	printf("New MODEL ID from image: %s\n", image_model_id);

	printf("Updating MODEL ID\n");
	set_board_data(BOARD_MODEL_ID_OFFSET, BOARD_MODEL_ID_LENGTH, image_model_id);

	printf("done\n\n");
}

#if defined(OPEN_SOURCE_ROUTER_SUPPORT) && defined(OPEN_SOURCE_ROUTER_ID)
int  image_match_open_source_fw_id (ulong fw_image_addr)
{
	char image_model_id[BOARD_MODEL_ID_LENGTH + 1];

	/*get hardward id from image */
	memset(image_model_id, 0, sizeof(image_model_id));
	board_get_image_info(fw_image_addr, "device", (char*)image_model_id);
	printf("MODEL ID on image: %s\n", image_model_id);

	if (strcmp(image_model_id, OPEN_SOURCE_ROUTER_ID) != 0) {
		printf("Firmware Image MODEL ID do not match open source firmware ID\n");
		return 0;
	}
	printf("Firmware Image MODEL ID matched open source firmware ID\n\n");
	return 1;
}
#endif
#endif	/* BOARD_ID */

void board_upgrade_string_table(unsigned char *load_addr, int table_number, unsigned int file_size)
{
	unsigned char *string_table_addr, *addr2, val2;
	unsigned long offset;
	unsigned int table_length;
	int offset_num;
	uchar *src_addr;
	ulong target_addr;
	char runcmd[256];

	/* Read whole string table partition from Flash to RAM (load_addr + CONFIG_SYS_STRING_TABLE_LEN)
	which is just after new string table sent by NMRP server. */
	string_table_addr = load_addr + CONFIG_SYS_STRING_TABLE_LEN;
	memset(string_table_addr, 0, CONFIG_SYS_STRING_TABLE_TOTAL_LEN);
	snprintf(runcmd, sizeof(runcmd), "nand read 0x%lx 0x%lx 0x%lx", string_table_addr, CONFIG_SYS_STRING_TABLE_BASE_ADDR, CONFIG_SYS_STRING_TABLE_TOTAL_LEN);
	run_command(runcmd, 0);

	/* Save string table checksum to (CONFIG_SYS_STRING_TABLE_LEN - 1) */
	memcpy(load_addr + CONFIG_SYS_STRING_TABLE_LEN - 1, load_addr + file_size- 1, 1);
	/* Remove checksum from string table file's tail */
	memset(load_addr + file_size - 1, 0, 1);

	table_length = file_size - 1;
	printf("string table length is %d\n", table_length);

	/* Save (string table length / 1024) to (CONFIG_SYS_STRING_TABLE_LEN-4) */
	val2 = table_length / 1024;
	addr2 = load_addr + CONFIG_SYS_STRING_TABLE_LEN - 4;
	memcpy(addr2, &val2, sizeof(val2));

	/* Save ((string table length % 1024) / 256) to (CONFIG_SYS_STRING_TABLE_LEN-3) */
	val2 = (table_length % 1024) / 256;
	addr2 = load_addr + CONFIG_SYS_STRING_TABLE_LEN - 3;
	memcpy(addr2, &val2, sizeof(val2));

	/* Save ((string table length % 1024) % 256) to (CONFIG_SYS_STRING_TABLE_LEN-2) */
	val2 = (table_length % 1024) % 256;
	addr2 = load_addr + CONFIG_SYS_STRING_TABLE_LEN - 2;
	memcpy(addr2, &val2, sizeof(val2));

	/* Copy processed string table from load_addr to RAM */
	offset = (table_number - 1) * CONFIG_SYS_STRING_TABLE_LEN;
	memcpy(string_table_addr + offset, load_addr, CONFIG_SYS_STRING_TABLE_LEN);

	/* Write back string tables from RAM to Flash */
	printf("erase from %x, length %x\n", CONFIG_SYS_STRING_TABLE_BASE_ADDR, CONFIG_SYS_STRING_TABLE_TOTAL_LEN);
	snprintf(runcmd, sizeof(runcmd), "nand erase 0x%lx 0x%lx", CONFIG_SYS_STRING_TABLE_BASE_ADDR, CONFIG_SYS_STRING_TABLE_TOTAL_LEN);
	run_command(runcmd, 0);

	CheckNmrpAliveTimerExpire(1);

	printf ("Copy all string tables to Flash...\n");
	for (offset_num = 0; offset_num < (CONFIG_SYS_STRING_TABLE_TOTAL_LEN / CONFIG_SYS_FLASH_SECTOR_SIZE); offset_num++) {
		src_addr = string_table_addr + offset_num * CONFIG_SYS_FLASH_SECTOR_SIZE;
		target_addr = CONFIG_SYS_STRING_TABLE_BASE_ADDR + offset_num * CONFIG_SYS_FLASH_SECTOR_SIZE;
		snprintf(runcmd, sizeof(runcmd), "nand write 0x%lx 0x%lx 0x%lx", src_addr, target_addr, CONFIG_SYS_FLASH_SECTOR_SIZE);
		run_command(runcmd, 0);

		CheckNmrpAliveTimerExpire(1);
	}
	return;
}

void board_reset_default_LedSet(void)
{
	static int DiagnosLedCount = 1;
	if ((DiagnosLedCount++ % 2) == 1) {
		/*power on test led 0.25s */
		board_test_led(0);
		net_set_timeout_handler((CONFIG_SYS_HZ* 1) / 4, board_reset_default_LedSet);
	} else {
		/*power off test led 0.75s */
		board_test_led(1);
		net_set_timeout_handler((CONFIG_SYS_HZ * 3) / 4, board_reset_default_LedSet);
	}
}

void board_reset_default_LedSet_slow(void)
{
        static int DiagnosLedCount = 1;
        if ((DiagnosLedCount++ % 2) == 1) {
                /*power on test led 1s */
                board_test_led(0);
                net_set_timeout_handler( CONFIG_SYS_HZ , board_reset_default_LedSet_slow);
        } else {
                /*power off test led 1s */
                board_test_led(1);
                net_set_timeout_handler( CONFIG_SYS_HZ , board_reset_default_LedSet_slow);
        }
}


/*erase the config sector of flash*/
void board_reset_default(void)
{
	int ret = 0;

	printf("Restore to factory default\n");

	char runcmd[256];
	printf("nand erase 0x%lx 0x%lx", CONFIG_SYS_FLASH_CONFIG_BASE, CONFIG_SYS_FLASH_SECTOR_SIZE);
	snprintf(runcmd, sizeof(runcmd), "nand erase 0x%lx 0x%lx", CONFIG_SYS_FLASH_CONFIG_BASE, CONFIG_SYS_FLASH_SECTOR_SIZE);
	run_command(runcmd,0);

#ifdef CONFIG_SYS_NMRP
	if(NmrpState != 0)
		return;
#endif
	printf("Rebooting...\n");
	do_reset(NULL,0,0,NULL);
}

int do_button_test (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
       if(!board_reset_button_is_press())
               printf("RESET button: NOT pressed\n");
       else
               printf("RESET button: Pressed\n");

       if(!board_wps_button_is_press())
               printf("WPS button: NOT pressed\n");
       else
               printf("WPS button: Pressed\n");

       return 0;
}

U_BOOT_CMD(
	button_test,1 , 1, do_button_test,
	"Test buttons",
	"- Test buttons\n"
	"Press and hold button to be tested before issuing this command.\n"
);


int do_upgrade_uboot (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	ulong load_addr = 0x42000000;
	char runcmd[256];

	printf("Starting to upgrade DNI u-boot...\n");
	snprintf(runcmd, sizeof(runcmd), "setenv machid 8010000");
	run_command(runcmd, 0);
	snprintf(runcmd, sizeof(runcmd), "tftpboot 0x%lx $uboot_name", load_addr);
	run_command(runcmd, 0);
	snprintf(runcmd, sizeof(runcmd), "crc32 0x%lx ${filesize}", load_addr);
	run_command(runcmd, 0);
	snprintf(runcmd, sizeof(runcmd), "imgaddr=0x42000000 && source $imgaddr:script");
	run_command(runcmd, 0);
	return 0;
}

U_BOOT_CMD(
	upgrade_uboot, 1 , 1, do_upgrade_uboot,
	"Upgrade u-boot",
	"- Upgrade u-boot\n"
	"Use this command to upgrade u-boot. You need to set file name by command \"setenv uboot_name FILE_NAME\"\n"
);

int do_delenv (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{	
	char runcmd[256];
	snprintf(runcmd, sizeof(runcmd), "mmc erase 0x3022 0x200");
	run_command(runcmd, 0);
	return 0;
}

U_BOOT_CMD(
	delenv, 1 , 1, do_delenv,
	"Erase uboot env variables",
	"- Erase uboot env variables\n"
	"Use this command to erase uboot env variables\n"
);

int do_uboot_version (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
#ifdef CONFIG_DISPLAY_BOARDINFO
    checkboard();
#endif
	return 0;
}

U_BOOT_CMD(
	uboot_version, 1, 1, do_uboot_version,
	"Show uboot version",
	"- Show uboot version\n"
	"Show the version information of u-boot\n"
);

int do_upload_vpn (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	ulong load_addr = 0x42000000;
	ulong vpn_cert_addr = CONFIG_SYS_VPN_BASE_ADDR;
	char runcmd[256];

	printf("Starting to upload VPN\n");
	snprintf(runcmd, sizeof(runcmd), "tftpboot 0x%lx $vpn_name", load_addr);
	run_command(runcmd, 0);
	snprintf(runcmd, sizeof(runcmd), "nand erase 0x%lx 0x%lx",  vpn_cert_addr, CONFIG_SYS_FLASH_SECTOR_SIZE);
	run_command(runcmd, 0);
//	snprintf(runcmd, sizeof(runcmd), "nand write 0x%lx 0x%lx ${filesize}", load_addr, vpn_cert_addr);
	snprintf(runcmd, sizeof(runcmd), "nand write 0x%lx 0x%lx 0x%lx", load_addr, vpn_cert_addr, CONFIG_SYS_FLASH_SECTOR_SIZE);
	run_command(runcmd, 0);
	return 0;
}

U_BOOT_CMD(
	upload_vpn, 1 , 1, do_upload_vpn,
	"Upload vpn",
	"- Upgload vpn\n"
	"Use this command to upload vpn. You need to set file name by command \"setenv vpn_name FILE_NAME\"\n"
);

int do_dump_mtdoops (cmd_tbl_t *cmdtp, int flag, int argc, char *argv[])
{
	ulong load_addr = 0x42000000;
	char runcmd[256];

	printf("Starting to dump mtdoops partition\n");
	snprintf(runcmd, sizeof(runcmd), "nand read 0x%lx 0x%lx 0x%lx", load_addr, CONFIG_SYS_MTDOOPS_BASE_ADDR, CONFIG_SYS_MTDOOPS_LEN);
	run_command(runcmd, 0);
	snprintf(runcmd, sizeof(runcmd), "md 0x%lx 0x%lx", load_addr, CONFIG_SYS_MTDOOPS_LEN/4);
	run_command(runcmd, 0);
	return 0;
}

U_BOOT_CMD(
	dump_mtdoops, 1 , 1, do_dump_mtdoops,
	"Dump mtdoops partition",
	"- Dump mtdoops\n"
	"Dump the whole mtdoops partition\n"
);


int do_dnibootm(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	char cmdbuf[256];

	snprintf(cmdbuf, sizeof(cmdbuf), "setenv fdt_high 0x4A600000");
	run_command(cmdbuf, 0);
	snprintf(cmdbuf, sizeof(cmdbuf), "setenv mtdids nand0=nand0;");
	run_command(cmdbuf, 0);

#ifdef CONFIG_USB_XHCI_IPQ
	int i;
	usb_stop();
	for (i=0; i<CONFIG_USB_MAX_CONTROLLER_COUNT; i++) {
		board_usb_deinit(i);
	}
#endif

	snprintf(cmdbuf, sizeof(cmdbuf), "bootm 0x44000000#config@hk01");
	run_command(cmdbuf, 0);
	return 0;
}

U_BOOT_CMD(
	dnibootm, 1, 0, do_dnibootm,
	"do necessary preparations, then bootm.",
	"\n"
);

/**
 * Set the uuid in bootargs variable for mounting rootfilesystem
 */
int set_uuid_bootargs(char *boot_args, char *part_name, int buflen, bool gpt_flag)
{   
    int ret, len;
    block_dev_desc_t *blk_dev;
    disk_partition_t disk_info;
    
    blk_dev = mmc_get_dev(mmc_host.dev_num);
    if (!blk_dev) {
        printf("Invalid block device name\n");
        return -EINVAL;
    }
    
    if (buflen <= 0 || buflen > MAX_BOOT_ARGS_SIZE)
        return -EINVAL;

#ifdef CONFIG_PARTITION_UUIDS
    ret = get_partition_info_efi_by_name(blk_dev,
            part_name, &disk_info);
    if (ret) {
        printf("bootipq: unsupported partition name %s\n",part_name);
        return -EINVAL;
    }
    if ((len = strlcpy(boot_args, "root=PARTUUID=", buflen)) >= buflen)
        return -EINVAL;
#else
    if ((len = strlcpy(boot_args, "rootfsname=", buflen)) >= buflen)
        return -EINVAL;
#endif
    boot_args += len;
    buflen -= len;

#ifdef CONFIG_PARTITION_UUIDS
    if ((len = strlcpy(boot_args, disk_info.uuid, buflen)) >= buflen)
        return -EINVAL;
#else
    if ((len = strlcpy(boot_args, part_name, buflen)) >= buflen)
        return -EINVAL;
#endif
    boot_args += len;
    buflen -= len;
    
    if (gpt_flag && strlcpy(boot_args, " gpt", buflen) >= buflen)
        return -EINVAL;
    
    return 0;
}

int is_secondary_core_off(unsigned int cpuid)
{
	int err;

	err = __invoke_psci_fn_smc(ARM_PSCI_TZ_FN_AFFINITY_INFO, cpuid, 0, 0);

	return err;
}

void bring_secondary_core_down(unsigned int state)
{
	__invoke_psci_fn_smc(ARM_PSCI_TZ_FN_CPU_OFF, state, 0, 0);
}

int bring_sec_core_up(unsigned int cpuid, unsigned int entry, unsigned int arg)
{
	int err;

	err = __invoke_psci_fn_smc(ARM_PSCI_TZ_FN_CPU_ON, cpuid, entry, arg);
	if (err) {
		printf("Enabling CPU%d via psci failed!\n", cpuid);
		return -1;
	}

	printf("Enabled CPU%d via psci successfully!\n", cpuid);
	return 0;
}

void run_tzt(void *address)
{
	execute_tzt(address);
}

void set_platform_specific_default_env(void)
{
	uint32_t soc_ver_major, soc_ver_minor, soc_version;
	int ret;

	ret = ipq_smem_get_socinfo_version((uint32_t *)&soc_version);
	if (!ret) {
		soc_ver_major = SOCINFO_VERSION_MAJOR(soc_version);
		soc_ver_minor = SOCINFO_VERSION_MINOR(soc_version);
		setenv_ulong("soc_version_major", (unsigned long)soc_ver_major);
		setenv_ulong("soc_version_minor", (unsigned long)soc_ver_minor);
	}
}

void sdi_disable(void)
{
	qca_scm_sdi();
}

int mmc_sect_erase_dni (unsigned int addr, unsigned int cnt) 
{
    unsigned int block;
    block = addr/0x200;
    cnt = cnt/0x200;
    if(cnt==0)
        cnt++;
    char runcmd[256];

    printf ("mmc_sect_erase will run command: mmc erase 0x%lx 0x%lx\n", block, cnt);
    snprintf(runcmd, sizeof(runcmd), "mmc erase 0x%lx 0x%lx",
                    block, cnt);
    
    if (run_command(runcmd, 0) != CMD_RET_SUCCESS)
        printf ("mmc_sect_erase error when running command: %s\n", runcmd);
}

int mmc_write_dni (char *src, unsigned int addr, unsigned int cnt) 
{
//  src = 0x84000000;
    addr=addr/0x200;
    cnt=cnt/0x200;
    if(cnt==0)
        cnt++;
    char runcmd[256];
      
    printf ("mmc_write will run command: mmc write 0x%lx 0x%lx 0x%lx\n", src, addr, cnt);
    snprintf(runcmd, sizeof(runcmd), "mmc write 0x%lx 0x%lx 0x%lx", src, addr, cnt);
    if (run_command(runcmd, 0) != CMD_RET_SUCCESS)
        printf ("mmc_write error when running command: %s\n", runcmd);
}

int mmc_read_dni (char *src, unsigned int addr, unsigned int cnt) 
{
//  src = 0x84000000;
    addr=addr/0x200;
    cnt=cnt/0x200;
    if(cnt==0)
        cnt++;
    char runcmd[256];

    printf ("mmc_read will run command: mmc read 0x%lx 0x%lx 0x%lx\n", src, addr, cnt);
    snprintf(runcmd, sizeof(runcmd), "mmc read 0x%lx 0x%lx 0x%lx", src, addr, cnt);
    if (run_command(runcmd, 0) != CMD_RET_SUCCESS){
        printf ("mmc_read error when running command: %s\n", runcmd);
    }    
}

int do_reset_i2c(cmd_tbl_t *cmdtp, int flag, int argc,
                              char * const argv[])
{   
    char runcmd[256];

    snprintf(runcmd, sizeof(runcmd), "i2c dev 0");
    run_command(runcmd, 0);
    snprintf(runcmd, sizeof(runcmd), "i2c probe");
    run_command(runcmd, 0);
    snprintf(runcmd, sizeof(runcmd), "i2c mw 0x27 0x0 0x01");
    run_command(runcmd, 0);
    snprintf(runcmd, sizeof(runcmd), "i2c mw 0x27 0x1 0x02");
    run_command(runcmd, 0);
    snprintf(runcmd, sizeof(runcmd), "i2c mw 0x27 0x2 0xff");
    run_command(runcmd, 0);
    snprintf(runcmd, sizeof(runcmd), "i2c mw 0x27 0x3 0xff");
    run_command(runcmd, 0);
    snprintf(runcmd, sizeof(runcmd), "i2c mw 0x27 0x4 0xff");
    run_command(runcmd, 0);
    snprintf(runcmd, sizeof(runcmd), "i2c mw 0x27 0x5 0xff");
    run_command(runcmd, 0);
    snprintf(runcmd, sizeof(runcmd), "i2c mw 0x27 0x6 0xff");
    run_command(runcmd, 0);
    snprintf(runcmd, sizeof(runcmd), "i2c mw 0x27 0x7 0xff");
    run_command(runcmd, 0);
    snprintf(runcmd, sizeof(runcmd), "i2c mw 0x27 0x8 0xff");
    run_command(runcmd, 0);
    snprintf(runcmd, sizeof(runcmd), "i2c mw 0x27 0x9 0xff");
    run_command(runcmd, 0);
    snprintf(runcmd, sizeof(runcmd), "i2c mw 0x27 0xa 0xff");
    run_command(runcmd, 0);
    snprintf(runcmd, sizeof(runcmd), "i2c mw 0x27 0xb 0x00");
    run_command(runcmd, 0);
    snprintf(runcmd, sizeof(runcmd), "i2c mw 0x27 0xc 0x55");
    run_command(runcmd, 0);
    snprintf(runcmd, sizeof(runcmd), "i2c mw 0x27 0xd 0x55");
    run_command(runcmd, 0);
    snprintf(runcmd, sizeof(runcmd), "i2c mw 0x27 0xe 0x92");
    run_command(runcmd, 0);
    snprintf(runcmd, sizeof(runcmd), "i2c mw 0x27 0xf 0x94");
    run_command(runcmd, 0);
    snprintf(runcmd, sizeof(runcmd), "i2c mw 0x27 0x10 0x98");
    run_command(runcmd, 0);
    snprintf(runcmd, sizeof(runcmd), "i2c mw 0x27 0x11 0xd0");
    run_command(runcmd, 0);

    return 0;
}

U_BOOT_CMD(
       reset_i2c, 1, 0, do_reset_i2c,
       "Reset I2C",
       "\n"
);

int do_reset_i2c_to_zero(cmd_tbl_t *cmdtp, int flag, int argc,
                                   char * const argv[])
{
    char runcmd[256];
    snprintf(runcmd, sizeof(runcmd), "i2c dev 0");
    run_command(runcmd, 0);
    snprintf(runcmd, sizeof(runcmd), "i2c probe");
    run_command(runcmd, 0);
    snprintf(runcmd, sizeof(runcmd), "i2c mw 0x27 0x0 0x01");
    run_command(runcmd, 0);
    snprintf(runcmd, sizeof(runcmd), "i2c mw 0x27 0x1 0x00");
    run_command(runcmd, 0);
    snprintf(runcmd, sizeof(runcmd), "i2c mw 0x27 0x2 0x00");
    run_command(runcmd, 0);
    snprintf(runcmd, sizeof(runcmd), "i2c mw 0x27 0x3 0x00");
    run_command(runcmd, 0);
    snprintf(runcmd, sizeof(runcmd), "i2c mw 0x27 0x4 0x00");
    run_command(runcmd, 0);
    snprintf(runcmd, sizeof(runcmd), "i2c mw 0x27 0x5 0x00");
    run_command(runcmd, 0);
    snprintf(runcmd, sizeof(runcmd), "i2c mw 0x27 0x6 0x00");
    run_command(runcmd, 0);
    snprintf(runcmd, sizeof(runcmd), "i2c mw 0x27 0x7 0x00");
    run_command(runcmd, 0);
    snprintf(runcmd, sizeof(runcmd), "i2c mw 0x27 0x8 0x00");
    run_command(runcmd, 0);
    snprintf(runcmd, sizeof(runcmd), "i2c mw 0x27 0x9 0x00");
    run_command(runcmd, 0);
    snprintf(runcmd, sizeof(runcmd), "i2c mw 0x27 0xa 0x00");
    run_command(runcmd, 0);
    snprintf(runcmd, sizeof(runcmd), "i2c mw 0x27 0xb 0x00");
    run_command(runcmd, 0);
    snprintf(runcmd, sizeof(runcmd), "i2c mw 0x27 0xc 0x00");
    run_command(runcmd, 0);
    snprintf(runcmd, sizeof(runcmd), "i2c mw 0x27 0xd 0x00");
    run_command(runcmd, 0);
    snprintf(runcmd, sizeof(runcmd), "i2c mw 0x27 0xe 0x92");
    run_command(runcmd, 0);
    snprintf(runcmd, sizeof(runcmd), "i2c mw 0x27 0xf 0x94");
    run_command(runcmd, 0);
    snprintf(runcmd, sizeof(runcmd), "i2c mw 0x27 0x10 0x98");
    run_command(runcmd, 0);
    snprintf(runcmd, sizeof(runcmd), "i2c mw 0x27 0x11 0xd0");
    run_command(runcmd, 0);

    return 0;
}

U_BOOT_CMD(
       reset_i2c_to_zero, 1, 0, do_reset_i2c_to_zero,
       "Reset I2C",
       "\n"
);

int do_reset_i2c_to_blink(cmd_tbl_t *cmdtp, int flag, int argc,
                              char * const argv[])
{
    char runcmd[256];
    snprintf(runcmd, sizeof(runcmd), "i2c dev 0");
    run_command(runcmd, 0);
    snprintf(runcmd, sizeof(runcmd), "i2c probe");
    run_command(runcmd, 0);
    snprintf(runcmd, sizeof(runcmd), "i2c mw 0x27 0x0 0x01");
    run_command(runcmd, 0);
    snprintf(runcmd, sizeof(runcmd), "i2c mw 0x27 0x1 0x22");
    run_command(runcmd, 0);
    snprintf(runcmd, sizeof(runcmd), "i2c mw 0x27 0x2 0xff");
    run_command(runcmd, 0);
    snprintf(runcmd, sizeof(runcmd), "i2c mw 0x27 0x3 0xff");
    run_command(runcmd, 0);
    snprintf(runcmd, sizeof(runcmd), "i2c mw 0x27 0x4 0xff");
    run_command(runcmd, 0);
    snprintf(runcmd, sizeof(runcmd), "i2c mw 0x27 0x5 0xff");
    run_command(runcmd, 0);
    snprintf(runcmd, sizeof(runcmd), "i2c mw 0x27 0x6 0xff");
    run_command(runcmd, 0);
    snprintf(runcmd, sizeof(runcmd), "i2c mw 0x27 0x7 0xff");
    run_command(runcmd, 0);
    snprintf(runcmd, sizeof(runcmd), "i2c mw 0x27 0x8 0xff");
    run_command(runcmd, 0);
    snprintf(runcmd, sizeof(runcmd), "i2c mw 0x27 0x9 0xff");
    run_command(runcmd, 0);
    snprintf(runcmd, sizeof(runcmd), "i2c mw 0x27 0xa 0x7f");
    run_command(runcmd, 0);
    snprintf(runcmd, sizeof(runcmd), "i2c mw 0x27 0xb 0x3b");
    run_command(runcmd, 0);
    snprintf(runcmd, sizeof(runcmd), "i2c mw 0x27 0xc 0xff");
    run_command(runcmd, 0);
    snprintf(runcmd, sizeof(runcmd), "i2c mw 0x27 0xd 0xff");
    run_command(runcmd, 0);
    snprintf(runcmd, sizeof(runcmd), "i2c mw 0x27 0xe 0x92");
    run_command(runcmd, 0);
    snprintf(runcmd, sizeof(runcmd), "i2c mw 0x27 0xf 0x94");
    run_command(runcmd, 0);
    snprintf(runcmd, sizeof(runcmd), "i2c mw 0x27 0x10 0x98");
    run_command(runcmd, 0);
    snprintf(runcmd, sizeof(runcmd), "i2c mw 0x27 0x11 0xd0");
    run_command(runcmd, 0);

    return 0;
}

U_BOOT_CMD(
       reset_i2c_to_blink, 1, 0, do_reset_i2c_to_blink,
       "Reset I2C to blink",
       "\n"
);
