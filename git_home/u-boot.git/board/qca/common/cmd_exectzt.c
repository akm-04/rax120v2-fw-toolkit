/*
 * Copyright (c) 2015-2017 The Linux Foundation. All rights reserved.
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
#include <command.h>
#include <asm/arch-qca-common/scm.h>

#if defined(CONFIG_HW29765960P0P4000P1000P4X4P4X4P4X4) || \
    defined(CONFIG_HW29765589P0P512P1024P4X4P8X8) || \
    defined(CONFIG_HW29766094P0P256P512P2X2P2X2) || \
    defined(CONFIG_HW29766097P0P512P1024P2X2P2X2P2X2) || \
    defined(CONFIG_HW29766193P0P256P512P2X2P2X2)
__weak void run_tzt(void *address)
{
	char runcmd[128];

	dcache_disable();
	snprintf(runcmd, sizeof(runcmd), "go 0x%08lX", (ulong)address);
	run_command(runcmd, 0);
}
#endif

int do_exectzt(cmd_tbl_t *cmdtp, int flag, int argc, char *const argv[])
{
	uint32_t address;

	if (argc != 2) {
		printf("No Arguments provided\n");
		printf("Command format: exectzt <address>\n");
		return 1;
	}

	address = simple_strtoul(argv[1], NULL, 16);

#if defined(CONFIG_HW29765960P0P4000P1000P4X4P4X4P4X4) || \
    defined(CONFIG_HW29765589P0P512P1024P4X4P8X8) || \
    defined(CONFIG_HW29766094P0P256P512P2X2P2X2) || \
    defined(CONFIG_HW29766097P0P512P1024P2X2P2X2P2X2) || \
    defined(CONFIG_HW29766193P0P256P512P2X2P2X2)
	run_tzt((void *)address);
#else
	execute_tzt(address);
#endif

	return 0;
}

U_BOOT_CMD(exectzt, 2, 0, do_exectzt,
		"execute TZT\n",
		"exectzt [address]  - Execute TZT\n");
