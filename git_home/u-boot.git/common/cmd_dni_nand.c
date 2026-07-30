/*
 * (C) Copyright 2001
 * Gerald Van Baren, Custom IDEAS, vanbaren@cideas.com
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

/*
 * DNI Utilities
 */

#include <common.h>
#include <command.h>
#include <miiphy.h>
#include <nand.h>
#include <errno.h>
#ifdef FIRMWARE_RECOVER_FROM_TFTP_SERVER
#include <dni_common.h>
#endif

#ifdef BOARDCAL

void get_board_data(int offset, int len, u8* buf)
{
	size_t read_size = CONFIG_SYS_FLASH_SECTOR_SIZE;
	unsigned char buffer[CONFIG_SYS_FLASH_SECTOR_SIZE];
	int ret = 0;
	loff_t current_offset = (loff_t)BOARDCAL;

#if defined(CONFIG_HW29765589P0P512P1024P4X4P8X8) || \
    defined(CONFIG_HW29766094P0P256P512P2X2P2X2) || \
    defined(CONFIG_HW29766097P0P512P1024P2X2P2X2P2X2)
	while (nand_block_isbad(&nand_info[0], current_offset)) {
		printf("Skipping bad block 0x%08llx\n", current_offset);
		current_offset += CONFIG_SYS_FLASH_SECTOR_SIZE;
	}
#endif
	ret = nand_read_skip_bad(&nand_info[0], current_offset, &read_size, NULL, CONFIG_SYS_FLASH_SECTOR_SIZE, (u_char *)buffer);
	printf(" %zu bytes read: %s\n", read_size,
	       ret ? "ERROR" : "OK");

	memcpy(buf, (void *)(buffer + offset), len);
}

/*function set_board_data()
 *description:
 *write data to the flash.
 * return value: 0 (success), 1 (fail)
 */
int set_board_data(int offset, int len, u8 *buf)
{
	char sectorBuff[CONFIG_SYS_FLASH_SECTOR_SIZE];
	size_t read_size = CONFIG_SYS_FLASH_SECTOR_SIZE;
	nand_erase_options_t nand_erase_options;
	size_t write_size = CONFIG_SYS_FLASH_SECTOR_SIZE;
	int ret = 0;
	loff_t current_offset = (loff_t)BOARDCAL;

#if defined(CONFIG_HW29765589P0P512P1024P4X4P8X8) || \
    defined(CONFIG_HW29766094P0P256P512P2X2P2X2) || \
    defined(CONFIG_HW29766097P0P512P1024P2X2P2X2P2X2)
	while (nand_block_isbad(&nand_info[0], current_offset)) {
		printf("Skipping bad block 0x%08llx\n", current_offset);
		current_offset += CONFIG_SYS_FLASH_SECTOR_SIZE;
	}
#endif
	ret = nand_read_skip_bad(&nand_info[0], current_offset, &read_size, NULL, CONFIG_SYS_FLASH_SECTOR_SIZE, (u_char *)sectorBuff);
	printf(" %zu bytes read: %s\n", read_size,
	       ret ? "ERROR" : "OK");
	memcpy(sectorBuff + offset, buf, len);
	update_data(sectorBuff, CONFIG_SYS_FLASH_SECTOR_SIZE,
			current_offset, BOARDCAL_LEN, 0, 0);
	return 0;
}

#ifdef CONFIG_TRIPLE_MAC_ADDRESS
int do_triple_macset(cmd_tbl_t *cmdtp, int flag, int argc,
                       char * const argv[])
{
    char    mac[6] = {255, 255, 255, 255, 255, 255}; // 255*6 = 1530
    int     mac_offset, i=0, j=0, val=0, sum=0;

    if(3 != argc)
        goto error;

    if(0 == strcmp(argv[1],"lan"))
        mac_offset = LAN_MAC_OFFSET;
    else if(0 == strcmp(argv[1],"wan"))
        mac_offset = WAN_MAC_OFFSET;
    else if(0 == strcmp(argv[1],"wlan5g"))
        mac_offset = WLAN_MAC_OFFSET;
    else
    {
        printf("unknown interface: %s\n",argv[1]);
        return 1;
    }

    while(argv[2][i])
    {
        if(':' == argv[2][i])
        {
            mac[j++] = val;
            i++;
            sum += val;
            val = 0;
            continue;
        }
        if((argv[2][i] >= '0') && (argv[2][i] <= '9'))
            val = val*16 + (argv[2][i] - '0');
        else if((argv[2][i] >='a') && (argv[2][i] <= 'f'))
            val = val*16 + (argv[2][i] - 'a') + 10;
        else if((argv[2][i] >= 'A') && (argv[2][i] <= 'F'))
            val = val*16 + (argv[2][i] - 'A') + 10;
        else
            goto error;
        i++;
    }
    mac[j] = val;
    sum += val;

    if(j != 5  || 0 == sum || 1530 == sum)
        goto error;

    set_board_data(mac_offset, 6, mac);

    return 0;

error:
    printf("\nUBOOT-1.1.4 MACSET TOOL copyright.\n");
    printf("Usage:\n  macset lan(wan,wlan5g) address\n");
    printf("  For instance : macset lan 00:03:7F:EF:77:87\n");
    printf("  The MAC address can not be all 0x00 or all 0xFF\n");
    return 1;
}

U_BOOT_CMD(
    macset, 3, 0, do_triple_macset,
    "Set ethernet MAC address",
    "<interface> <address> - Program the MAC address of <interface>\n"
    "<interfcae> should be lan, wan or wlan5g\n"
    "<address> should be the format as 00:03:7F:EF:77:87"
);

int do_triple_macshow(cmd_tbl_t *cmdtp, int flag, int argc,
                        char * const argv[])
{
    unsigned char mac[18];

    get_board_data(0, 18, mac);
    
	printf("lan mac: %02x:%02x:%02x:%02x:%02x:%02x\n",mac[LAN_MAC_OFFSET],mac[LAN_MAC_OFFSET+1],mac[LAN_MAC_OFFSET+2],mac[LAN_MAC_OFFSET+3],mac[LAN_MAC_OFFSET+4],mac[LAN_MAC_OFFSET+5]);
    printf("wan mac: %02x:%02x:%02x:%02x:%02x:%02x\n",mac[WAN_MAC_OFFSET],mac[WAN_MAC_OFFSET+1],mac[WAN_MAC_OFFSET+2],mac[WAN_MAC_OFFSET+3],mac[WAN_MAC_OFFSET+4],mac[WAN_MAC_OFFSET+5]);
    printf("wlan5g mac: %02x:%02x:%02x:%02x:%02x:%02x\n",mac[WLAN_MAC_OFFSET],mac[WLAN_MAC_OFFSET+1],mac[WLAN_MAC_OFFSET+2],mac[WLAN_MAC_OFFSET+3],mac[WLAN_MAC_OFFSET+4],mac[WLAN_MAC_OFFSET+5]);
    return 0;
}

U_BOOT_CMD(
    macshow, 1, 0, do_triple_macshow,
    "Show ethernet MAC addresses",
    "Display all the ethernet MAC addresses\n"
    "          for instance: the MAC of lan and wan"
);
#endif

#ifdef CONFIG_QUADRUPLE_MAC_ADDRESS
int do_quadruple_macset(cmd_tbl_t *cmdtp, int flag, int argc,
                       char * const argv[])
{
    char    mac[6] = {255, 255, 255, 255, 255, 255}; // 255*6 = 1530
    int     mac_offset, i=0, j=0, val=0, sum=0;

    if(3 != argc)
        goto error;

    if(0 == strcmp(argv[1],"lan"))
        mac_offset = LAN_MAC_OFFSET;
    else if(0 == strcmp(argv[1],"wan"))
        mac_offset = WAN_MAC_OFFSET;
    else if(0 == strcmp(argv[1],"wlan5g"))
        mac_offset = WLAN_MAC_OFFSET;
	else if(0 == strcmp(argv[1],"bluetooth"))
		mac_offset = BLUETOOTH_MAC_OFFSET;
    else
    {
        printf("unknown interface: %s\n",argv[1]);
        return 1;
    }

    while(argv[2][i])
    {
        if(':' == argv[2][i])
        {
            mac[j++] = val;
            i++;
            sum += val;
            val = 0;
            continue;
        }
        if((argv[2][i] >= '0') && (argv[2][i] <= '9'))
            val = val*16 + (argv[2][i] - '0');
        else if((argv[2][i] >='a') && (argv[2][i] <= 'f'))
            val = val*16 + (argv[2][i] - 'a') + 10;
        else if((argv[2][i] >= 'A') && (argv[2][i] <= 'F'))
            val = val*16 + (argv[2][i] - 'A') + 10;
        else
            goto error;
        i++;
    }
    mac[j] = val;
    sum += val;

    if(j != 5  || 0 == sum || 1530 == sum)
        goto error;

    set_board_data(mac_offset, 6, mac);

    return 0;

error:
    printf("\nUBOOT-1.1.4 MACSET TOOL copyright.\n");
    printf("Usage:\n  macset lan(wan,wlan5g) address\n");
    printf("  For instance : macset lan 00:03:7F:EF:77:87\n");
    printf("  The MAC address can not be all 0x00 or all 0xFF\n");
    return 1;
}

U_BOOT_CMD(
    macset, 3, 0, do_quadruple_macset,
    "Set ethernet MAC address",
    "<interface> <address> - Program the MAC address of <interface>\n"
    "<interfcae> should be lan, wan or wlan5g\n"
    "<address> should be the format as 00:03:7F:EF:77:87"
);

int do_quadruple_macshow(cmd_tbl_t *cmdtp, int flag, int argc,
                        char * const argv[])
{
    unsigned char mac[24];

    get_board_data(0, 24, mac);
    
	printf("lan mac: %02x:%02x:%02x:%02x:%02x:%02x\n",mac[LAN_MAC_OFFSET],mac[LAN_MAC_OFFSET+1],mac[LAN_MAC_OFFSET+2],mac[LAN_MAC_OFFSET+3],mac[LAN_MAC_OFFSET+4],mac[LAN_MAC_OFFSET+5]);
    printf("wan mac: %02x:%02x:%02x:%02x:%02x:%02x\n",mac[WAN_MAC_OFFSET],mac[WAN_MAC_OFFSET+1],mac[WAN_MAC_OFFSET+2],mac[WAN_MAC_OFFSET+3],mac[WAN_MAC_OFFSET+4],mac[WAN_MAC_OFFSET+5]);
    printf("wlan5g mac: %02x:%02x:%02x:%02x:%02x:%02x\n",mac[WLAN_MAC_OFFSET],mac[WLAN_MAC_OFFSET+1],mac[WLAN_MAC_OFFSET+2],mac[WLAN_MAC_OFFSET+3],mac[WLAN_MAC_OFFSET+4],mac[WLAN_MAC_OFFSET+5]);
    printf("bluetooth mac: %02x:%02x:%02x:%02x:%02x:%02x\n",mac[BLUETOOTH_MAC_OFFSET],mac[BLUETOOTH_MAC_OFFSET+1],mac[BLUETOOTH_MAC_OFFSET+2],mac[BLUETOOTH_MAC_OFFSET+3],mac[BLUETOOTH_MAC_OFFSET+4],mac[BLUETOOTH_MAC_OFFSET+5]);
	return 0;
}

U_BOOT_CMD(
    macshow, 1, 0, do_quadruple_macshow,
    "Show ethernet MAC addresses",
    "Display all the ethernet MAC addresses\n"
    "          for instance: the MAC of lan and wan"
);
#endif

#ifdef CONFIG_QUINTUPLE_MAC_ADDRESS
int do_quintuple_macset(cmd_tbl_t *cmdtp, int flag, int argc,
                       char * const argv[])
{
    char    mac[6] = {255, 255, 255, 255, 255, 255}; // 255*6 = 1530
    int     mac_offset, i=0, j=0, val=0, sum=0;

    if(3 != argc)
        goto error;

    if(0 == strcmp(argv[1],"lan"))
        mac_offset = LAN_MAC_OFFSET;
    else if(0 == strcmp(argv[1],"wan"))
        mac_offset = WAN_MAC_OFFSET;
    else if(0 == strcmp(argv[1],"wlan5g"))
        mac_offset = WLAN_MAC_OFFSET;
    else if(0 == strcmp(argv[1],"wlan2nd5g"))
        mac_offset = WLAN_2nd5G_MAC_OFFSET;
    else if(0 == strcmp(argv[1],"bt"))
        mac_offset = BT_MAC_OFFSET;
    else 
    {    
        printf("unknown interface: %s\n",argv[1]);
        return 1;
    }    

    while(argv[2][i])
    {    
        if(':' == argv[2][i])
        {
            mac[j++] = val; 
            i++;
            sum += val; 
            val = 0; 
            continue;
        }
        if((argv[2][i] >= '0') && (argv[2][i] <= '9'))
            val = val*16 + (argv[2][i] - '0');
        else if((argv[2][i] >='a') && (argv[2][i] <= 'f'))
            val = val*16 + (argv[2][i] - 'a') + 10;
        else if((argv[2][i] >= 'A') && (argv[2][i] <= 'F'))
            val = val*16 + (argv[2][i] - 'A') + 10;
        else
            goto error;
        i++;
    }
    mac[j] = val; 
    sum += val; 

    if(j != 5  || 0 == sum || 1530 == sum)
        goto error;

    set_board_data(mac_offset, 6, mac);
    return 0;

error:
    printf("\nUBOOT-1.1.4 MACSET TOOL copyright.\n");
    printf("Usage:\n  macset lan(wan,wlan5g,wlan2nd5g,bt) address\n");
    printf("  For instance : macset lan 00:03:7F:EF:77:87\n");
    printf("  The MAC address can not be all 0x00 or all 0xFF\n");
    return 1;
}

U_BOOT_CMD(
    macset, 3, 0, do_quintuple_macset,
    "Set ethernet MAC address",
    "<interface> <address> - Program the MAC address of <interface>\n"
    "<interfcae> should be lan, wan, wlan5g, wlan2nd5g or bt\n"
    "<address> should be the format as 00:03:7F:EF:77:87"
);

int do_quintuple_macshow(cmd_tbl_t *cmdtp, int flag, int argc,
                        char * const argv[])
{
    unsigned char mac[30];

    get_board_data(0, 30, mac);
    
	printf("lan mac: %02x:%02x:%02x:%02x:%02x:%02x\n",mac[LAN_MAC_OFFSET],mac[LAN_MAC_OFFSET+1],mac[LAN_MAC_OFFSET+2],mac[LAN_MAC_OFFSET+3],mac[LAN_MAC_OFFSET+4],mac[LAN_MAC_OFFSET+5]);
    printf("wan mac: %02x:%02x:%02x:%02x:%02x:%02x\n",mac[WAN_MAC_OFFSET],mac[WAN_MAC_OFFSET+1],mac[WAN_MAC_OFFSET+2],mac[WAN_MAC_OFFSET+3],mac[WAN_MAC_OFFSET+4],mac[WAN_MAC_OFFSET+5]);
    printf("wlan5g mac: %02x:%02x:%02x:%02x:%02x:%02x\n",mac[WLAN_MAC_OFFSET],mac[WLAN_MAC_OFFSET+1],mac[WLAN_MAC_OFFSET+2],mac[WLAN_MAC_OFFSET+3],mac[WLAN_MAC_OFFSET+4],mac[WLAN_MAC_OFFSET+5]);
    printf("wlan2nd5g mac: %02x:%02x:%02x:%02x:%02x:%02x\n",mac[WLAN_2nd5G_MAC_OFFSET],mac[WLAN_2nd5G_MAC_OFFSET+1],mac[WLAN_2nd5G_MAC_OFFSET+2],mac[WLAN_2nd5G_MAC_OFFSET+3],mac[WLAN_2nd5G_MAC_OFFSET+4],mac[WLAN_2nd5G_MAC_OFFSET+5]);
    printf("bt mac: %02x:%02x:%02x:%02x:%02x:%02x\n",mac[BT_MAC_OFFSET],mac[BT_MAC_OFFSET+1],mac[BT_MAC_OFFSET+2],mac[BT_MAC_OFFSET+3],mac[BT_MAC_OFFSET+4],mac[BT_MAC_OFFSET+5]);
    return 0;
}

U_BOOT_CMD(
    macshow, 1, 0, do_quintuple_macshow,
    "Show ethernet MAC addresses",
    "Display all the ethernet MAC addresses\n"
    "          for instance: the MAC of lan and wan"
);
#endif

#if defined(WPSPIN_OFFSET) && defined(WPSPIN_LENGTH)
int do_wpspinset(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	char wpspin[WPSPIN_LENGTH] = {0};

	if (2 != argc) {
		printf("%s\n", cmdtp->usage);
		return 1;
	}

	strncpy(wpspin, argv[1], WPSPIN_LENGTH);

	printf("Burn wpspin into ART block.\n");
	set_board_data(WPSPIN_OFFSET, WPSPIN_LENGTH, wpspin);

	puts ("done\n");
	return 0;
}

U_BOOT_CMD(
	wpspinset, 2, 0, do_wpspinset,
	"Set wpspin number",
	"number\n"
	" For instance: wpspinset 12345678"
);

#endif

#if defined(SERIAL_NUMBER_OFFSET) && defined(SERIAL_NUMBER_LENGTH)
/*function do_snset()
 *description:
 *write the Serial Number to the flash.
 * return value:
 * 0:success
 * 1:fail
 */
int do_snset(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	char sn[SERIAL_NUMBER_LENGTH] = {0};
	int sn_len = 0, i = 0;

	if (2 != argc) {
		printf("%s\n",cmdtp->usage);
		return 1;
	}

	sn_len = strlen(argv[1]);   /*check the SN's length*/
	if (sn_len != SERIAL_NUMBER_LENGTH) {
		printf ("SN's len is wrong,it's lenth is %d\n ", SERIAL_NUMBER_LENGTH);
		return 1;
	}

	strncpy(sn, argv[1], SERIAL_NUMBER_LENGTH);
	for (i=0; i<SERIAL_NUMBER_LENGTH; ++i)/*check seria naumber is 0~9 or A~Z*/
	{
		if (!(((sn[i]>=0x30) && (sn[i]<=0x39)) || ((sn[i]>=0x41) && (sn[i]<=0x5a))))    /*sn is 0~9 or A~Z*/
		{
			puts ("the SN only is 0~9 or A~Z\n");
			break;
		}
	}

	if (i < SERIAL_NUMBER_LENGTH)       /*because SN is not 0~9 or A~Z*/
		return 1;

	puts("Burn SN into ART block.\n");
	set_board_data(SERIAL_NUMBER_OFFSET, SERIAL_NUMBER_LENGTH, sn);

	puts("Done.\n");
	return 0;
}

U_BOOT_CMD(
	snset, 2, 0, do_snset,
	"Set serial number",
	"number (13 digit)\n"
	" For instance: snset 1ML1747D0000B"
);
#endif

#if defined(REGION_NUMBER_OFFSET) && defined(REGION_NUMBER_LENGTH)
/*function set_region()
 *description:
 *write the Region Number to the flash.
 * return value:
 * 0:success
 * 1:fail
 */
int set_region(u16 host_region_number)
{
	int rn_len = 0, i = 0;
	/* Always save region number as network order */
	u16 region_number = htons(host_region_number);

	puts("Burn Region Number into ART block.\n");
	set_board_data(REGION_NUMBER_OFFSET, REGION_NUMBER_LENGTH, &region_number);

	puts("Done.\n");
	return 0;
}

/*function do_rnset()
 *description:
 * read command input and translate to u16,
 * then call set_region() to write to flash.
 * return value:
 * 0:success
 * 1:fail
 */
int do_rnset(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	char *strtol_endptr = NULL;
	uint16_t region = 0;

	int i = 0;

	if (2 != argc) {
		printf("%s\n",cmdtp->usage);
		return 1;
	}

	region = (uint16_t)simple_strtoul(argv[1], &strtol_endptr, 10);
	if (*strtol_endptr != '\0') {
		printf("\"%s\" is not a number!!\n", argv[1]);
		return 1;
	}

	printf("write 0x%04x to board region\n", region);

	return set_region(region);

}

U_BOOT_CMD(
	rnset, 2, 0, do_rnset,
	"set region number",
	"number (2 digit)\n"
	" For instance: rnset 01"
);

int do_rnshow(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	u16 rn;

	get_board_data(REGION_NUMBER_OFFSET, sizeof(rn), &rn);
	
	printf("region on board: 0x%04x\n", ntohs(rn));
	return 0;
}

U_BOOT_CMD(
	rnshow, 1, 0, do_rnshow,
	"Show Region Number on Board",
	"\n"
	" For instance: rnshow"
);
#endif

#if defined(BOARD_HW_ID_OFFSET) && defined(BOARD_HW_ID_LENGTH)
/*function do_board_hw_id_set()
 *description:
 * read in board_hw_id, then call set_board_data() to write to flash.
 * return value: 0 (success), 1 (fail)
 */
int do_board_hw_id_set(cmd_tbl_t *cmdtp, int flag, int argc,
                       char * const argv[])
{
	u8 board_hw_id[BOARD_HW_ID_LENGTH + 1];
	int board_hw_id_len = 0;

	if (argc != 2) {
		printf("%s\n",cmdtp->usage);
		return 1;
	}
	if ((board_hw_id_len = strlen(argv[1])) > BOARD_HW_ID_LENGTH) {
		printf ("the length of BOARD_HW_ID can't > %d\n", BOARD_HW_ID_LENGTH);
		return 1;
	}

	memset(board_hw_id, 0, sizeof(board_hw_id));
	memcpy(board_hw_id, argv[1], board_hw_id_len);

	printf("Burn board_hw_id (= %s) into ART block\n", board_hw_id);
	set_board_data(BOARD_HW_ID_OFFSET, BOARD_HW_ID_LENGTH, board_hw_id);
	puts("Done.\n");
	return 0;
}

U_BOOT_CMD(
	board_hw_id_set, 2, 0, do_board_hw_id_set,
	"Set board_hw_id",
	"XXXXXX"
);

void get_board_hw_id(u8* buf) /* sizeof(buf) must > BOARD_HW_ID_LENGTH */
{
	get_board_data(BOARD_HW_ID_OFFSET, BOARD_HW_ID_LENGTH, buf);
}

int do_board_hw_id_show(cmd_tbl_t *cmdtp, int flag, int argc,
                        char * const argv[])
{
	u8 board_hw_id[BOARD_HW_ID_LENGTH + 1];

	memset(board_hw_id, 0, sizeof(board_hw_id));
	get_board_hw_id(board_hw_id);
	printf("board_hw_id : %s\n", board_hw_id);
	return 0;
}

U_BOOT_CMD(
	board_hw_id_show, 1, 0, do_board_hw_id_show,
	"Show board_hw_id",
	"\n"
	" For instance: board_hw_id_show"
);

#if defined(BOARD_MODEL_ID_OFFSET) && defined(BOARD_MODEL_ID_LENGTH)
/*function do_board_model_id_set()
 *description:
 * read in board_model_id, then call set_board_data() to write to flash.
 * return value: 0 (success), 1 (fail)
 */
int do_board_model_id_set(cmd_tbl_t *cmdtp, int flag, int argc,
                          char * const argv[])
{
	u8 board_model_id[BOARD_MODEL_ID_LENGTH + 1];
	int board_model_id_len = 0;

	if (argc != 2) {
		printf("%s\n",cmdtp->usage);
		return 1;
	}
	if ((board_model_id_len = strlen(argv[1])) > BOARD_MODEL_ID_LENGTH) {
		printf ("the length of BOARD_MODEL_ID can't > %d\n", BOARD_MODEL_ID_LENGTH);
		return 1;
	}

	memset(board_model_id, 0, sizeof(board_model_id));
	memcpy(board_model_id, argv[1], board_model_id_len);

	printf("Burn board_model_id (= %s) into ART block\n", board_model_id);
	set_board_data(BOARD_MODEL_ID_OFFSET, BOARD_MODEL_ID_LENGTH, board_model_id);
	puts("Done.\n");
	return 0;
}

U_BOOT_CMD(
	board_model_id_set, 2, 0, do_board_model_id_set,
	"Set board_model_id",
	"XXXXXX"
);

void get_board_model_id(u8* buf) /* sizeof(buf) must > BOARD_MODEL_ID_LENGTH */
{
	get_board_data(BOARD_MODEL_ID_OFFSET, BOARD_MODEL_ID_LENGTH, buf);
}

int do_board_model_id_show(cmd_tbl_t *cmdtp, int flag, int argc,
                           char * const argv[])
{
	u8 board_model_id[BOARD_MODEL_ID_LENGTH + 1];

	memset(board_model_id, 0, sizeof(board_model_id));
	get_board_model_id(board_model_id);
	printf("board_model_id : %s\n", board_model_id);
	return 0;
}

U_BOOT_CMD(
	board_model_id_show, 1, 0, do_board_model_id_show,
	"Show board_model_id",
	"\n"
	" For instance: board_model_id_show"
);
#endif	/* MODEL_ID */
#endif	/* HW_ID */

#if defined(BOARD_SSID_OFFSET) && defined(BOARD_SSID_LENGTH)
/*function do_board_ssid_set()
 *description:
 * read in ssid, then call set_board_data() to write to flash.
 * return value: 0 (success), 1 (fail)
 */
int do_board_ssid_set(cmd_tbl_t *cmdtp, int flag, int argc, char *const argv[])
{
	u8 board_ssid[BOARD_SSID_LENGTH + 1];
	int board_ssid_len = 0;

	if (argc != 2) {
		printf("%s\n",cmdtp->usage);
		return 1;
	}
	if ((board_ssid_len = strlen(argv[1])) > BOARD_SSID_LENGTH) {
		printf ("the length of SSID can't > %d\n", BOARD_SSID_LENGTH);
		return 1;
	}

	memset(board_ssid, 0, sizeof(board_ssid));
	memcpy(board_ssid, argv[1], board_ssid_len);

	printf("Burn SSID (= %s) into ART block\n", board_ssid);
	set_board_data(BOARD_SSID_OFFSET, BOARD_SSID_LENGTH, board_ssid);
	puts("Done.\n");
	return 0;
}

U_BOOT_CMD(
	board_ssid_set, 2, 0, do_board_ssid_set,
	"Set ssid on board",
	"XXXXXX\n"
	" For instance: board_ssid_set NETGEAR"
);

void get_board_ssid(u8* buf) /* sizeof(buf) must > BOARD_SSID_LENGTH */
{
	get_board_data(BOARD_SSID_OFFSET, BOARD_SSID_LENGTH, buf);
}

int do_board_ssid_show(cmd_tbl_t *cmdtp, int flag, int argc,
                       char * const argv[])
{
	u8 board_ssid[BOARD_SSID_LENGTH + 1];

	memset(board_ssid, 0, sizeof(board_ssid));
	get_board_ssid(board_ssid);
	printf("board_ssid : %s\n", board_ssid);
	return 0;
}

U_BOOT_CMD(
	board_ssid_show, 1, 0, do_board_ssid_show,
	"Show board_ssid",
	"\n"
	" For instance: board_ssid_show"
);

#endif	/* BOARD_SSID */

#if defined(BOARD_PASSPHRASE_OFFSET) && defined(BOARD_PASSPHRASE_LENGTH)
/*function do_board_passphrase_set()
 *description:
 * read in passphrase, then call set_board_data() to write to flash.
 * return value: 0 (success), 1 (fail)
 */
int do_board_passphrase_set(cmd_tbl_t *cmdtp, int flag, int argc,
                            char * const argv[])
{
	u8 board_passphrase[BOARD_PASSPHRASE_LENGTH + 1];
	int board_passphrase_len = 0;

	if (argc != 2) {
		printf("%s\n",cmdtp->usage);
		return 1;
	}
	if ((board_passphrase_len = strlen(argv[1])) > BOARD_PASSPHRASE_LENGTH) {
		printf ("the length of PASSPHRASE can't > %d\n", BOARD_PASSPHRASE_LENGTH);
		return 1;
	}

	memset(board_passphrase, 0, sizeof(board_passphrase));
	memcpy(board_passphrase, argv[1], board_passphrase_len);

	printf("Burn PASSPHRASE (= %s) into ART block\n", board_passphrase);
	set_board_data(BOARD_PASSPHRASE_OFFSET, BOARD_PASSPHRASE_LENGTH, board_passphrase);
	puts("Done.\n");
	return 0;
}

U_BOOT_CMD(
	board_passphrase_set, 2, 0, do_board_passphrase_set,
	"Set passphrase on board",
	"XXXXXX\n"
	" For instance: board_passphrase_set 1234567890"
);

void get_board_passphrase(u8* buf) /* sizeof(buf) must > BOARD_PASSPHRASE_LENGTH */
{
	get_board_data(BOARD_PASSPHRASE_OFFSET, BOARD_PASSPHRASE_LENGTH, buf);
}

int do_board_passphrase_show(cmd_tbl_t *cmdtp, int flag, int argc,
                             char * const argv[])
{
	u8 board_passphrase[BOARD_PASSPHRASE_LENGTH + 1];

	memset(board_passphrase, 0, sizeof(board_passphrase));
	get_board_passphrase(board_passphrase);
	printf("board_passphrase : %s\n", board_passphrase);
	return 0;
}

U_BOOT_CMD(
	board_passphrase_show, 1, 0, do_board_passphrase_show,
	"Show board_passphrase",
	"\n"
	" For instance: board_passphrase_show"
);
#endif	/* BOARD_PASSPHRASE */

#if defined(CONFIG_CMD_BOARD_PARAMETERS) \
&& defined(LAN_MAC_OFFSET) && defined(WAN_MAC_OFFSET) \
&& defined(WPSPIN_LENGTH) && defined(WPSPIN_OFFSET) && defined(SERIAL_NUMBER_LENGTH) && defined(SERIAL_NUMBER_OFFSET) \
&& defined(REGION_NUMBER_LENGTH) && defined(REGION_NUMBER_OFFSET) && defined(BOARD_HW_ID_LENGTH) && defined(BOARD_HW_ID_OFFSET) \
&& defined(BOARD_MODEL_ID_LENGTH) && defined(BOARD_MODEL_ID_OFFSET) && defined(BOARD_SSID_LENGTH) && defined(BOARD_SSID_OFFSET) \
&& defined(BOARD_PASSPHRASE_LENGTH) && defined(BOARD_PASSPHRASE_OFFSET)
#if defined(CONFIG_TRIPLE_MAC_ADDRESS)
int do_board_parameters_set(cmd_tbl_t *cmdtp, int flag, int argc,
                            char * const argv[])
{
	char sectorBuff[6 * 3 +
         WPSPIN_LENGTH + SERIAL_NUMBER_LENGTH +
         REGION_NUMBER_LENGTH + BOARD_HW_ID_LENGTH +
         BOARD_MODEL_ID_LENGTH + BOARD_SSID_LENGTH +
         BOARD_PASSPHRASE_LENGTH 
#if defined(BLUETOOTH_MAC_OFFSET)
	+ BLUETOOTH_MAC_LENGTH
#endif

#if defined(SFP_MAC_OFFSET)
         +SFP_MAC_LENGTH 
#endif

#if defined(WLAN11AD_MAC_OFFSET)
	+WLAN11AD_MAC_LENGTH
#endif
	];

	u8 mac[6][6] = {
	    {255, 255, 255, 255, 255, 255},
                    {255, 255, 255, 255, 255, 255},
                    {255, 255, 255, 255, 255, 255},
                    {255, 255, 255, 255, 255, 255},
                    {255, 255, 255, 255, 255, 255},
                    {255, 255, 255, 255, 255, 255}
                 };


	char wpspin[WPSPIN_LENGTH] = {0};
	char sn[SERIAL_NUMBER_LENGTH] = {0};
	u8 board_ssid[BOARD_SSID_LENGTH + 1];
	u8 board_passphrase[BOARD_PASSPHRASE_LENGTH + 1];
	int wps_len = 0;
	int sn_len = 0;
	int board_passphrase_len = 0;
	int board_ssid_len = 0;
	int offset, i=0, j=0, k=0, val=0, sum=0, length, mac_num=2, m=0;

	if (argc > 11 || argc < 7) {
		printf("%s\n",cmdtp->usage);
		return 1;
	}
	if (argc >= 8)
		mac_num += argc -7 ;
	printf("mac_num:%d\n",mac_num);	

	length = 6 * mac_num + WPSPIN_LENGTH + SERIAL_NUMBER_LENGTH +
					REGION_NUMBER_LENGTH + BOARD_HW_ID_LENGTH +
					BOARD_MODEL_ID_LENGTH + BOARD_SSID_LENGTH +
					BOARD_PASSPHRASE_LENGTH ;

	/* Check WPS length */
	if ((wps_len = strlen(argv[1])) > WPSPIN_LENGTH)
	{
		printf ("the length of wpspin can't > %d\n", WPSPIN_LENGTH);
		return 1;
	}

	/* Check serial number */
	sn_len = strlen(argv[2]);   /*check the SN's length*/
	if (sn_len != SERIAL_NUMBER_LENGTH)
	{
		printf ("SN's len is wrong,it's lenth is %d\n ", SERIAL_NUMBER_LENGTH);
		return 1;
	}

	strncpy(sn, argv[2], SERIAL_NUMBER_LENGTH);
	for (i = 0; i < SERIAL_NUMBER_LENGTH; ++i)/*check seria naumber is 0~9 or A~Z*/
	{
		if (!(((sn[i]>=0x30) && (sn[i]<=0x39)) || ((sn[i]>=0x41) && (sn[i]<=0x5a))))    /*sn is 0~9 or A~Z*/
		{
			puts ("the SN only is 0~9 or A~Z\n");
			break;
		}
	}

	if (i < SERIAL_NUMBER_LENGTH)       /*because SN is not 0~9 or A~Z*/
		return 1;

	/* Check SSID length */
	if ((board_ssid_len = strlen(argv[3])) > BOARD_SSID_LENGTH)
	{
		printf ("the length of SSID can't > %d\n", BOARD_SSID_LENGTH);
		return 1;
	}

	/* Check Passphrase length */
	if ((board_passphrase_len = strlen(argv[4])) > BOARD_PASSPHRASE_LENGTH)
	{
		printf ("the length of PASSPHRASE can't > %d\n", BOARD_PASSPHRASE_LENGTH);
		return 1;
	}

	/* check MAC address */
	for (k = 5; k < 5 + mac_num; ++k)
	{
		sum = 0 , val = 0;
		i = 0; j = 0;
		while (argv[k][i])
		{
			if (':' == argv[k][i])
			{
				mac[m][j++] = val;
				i++;
				sum += val;
				val = 0;
				continue;
			}
			if ((argv[k][i] >= '0') && (argv[k][i] <= '9'))
				val = val*16 + (argv[k][i] - '0');
			else if ((argv[k][i] >='a') && (argv[k][i] <= 'f'))
				val = val*16 + (argv[k][i] - 'a') + 10;
			else if ((argv[k][i] >= 'A') && (argv[k][i] <= 'F'))
				val = val*16 + (argv[k][i] - 'A') + 10;
			else
			{
				printf("The %d MAC address is incorrect\n",k);
				printf("The MAC address can not be all 0x00 or all 0xFF\n");
				return 1;
			}
			i++;
		}
		mac[m][j] = val;
		sum += val;
		m++;
		if (j != 5  || 0 == sum || 1530 == sum)
		{
			printf("The %d MAC address is incorrect\n",k);
			printf("The MAC address can not be all 0x00 or all 0xFF\n");
			return 1;
		}
	}


	/* Copy new settings to buffer */
	get_board_data(0, length, sectorBuff);
	if(mac_num <= 3)
		memcpy(sectorBuff, mac, 6 * mac_num);
	else{
	        memcpy(sectorBuff, mac, 6 * 3);
		memcpy(sectorBuff + BLUETOOTH_MAC_OFFSET, mac[3], 6 * (mac_num-3) );
	}
	memset(wpspin, 0, sizeof(wpspin));
	memcpy(wpspin, argv[1], wps_len);
	memcpy(sectorBuff + WPSPIN_OFFSET, wpspin, WPSPIN_LENGTH);
	memcpy(sectorBuff + SERIAL_NUMBER_OFFSET, sn, SERIAL_NUMBER_LENGTH);
	memset(board_ssid, 0, sizeof(board_ssid));
	memcpy(board_ssid, argv[3], board_ssid_len);
	memcpy(sectorBuff + BOARD_SSID_OFFSET, board_ssid, BOARD_SSID_LENGTH);
	memset(board_passphrase, 0, sizeof(board_passphrase));
	memcpy(board_passphrase, argv[4], board_passphrase_len);
	memcpy(sectorBuff + BOARD_PASSPHRASE_OFFSET, board_passphrase, BOARD_PASSPHRASE_LENGTH);

	printf("Burn the following parameters into ART block.\n");
	printf("lan mac: %02X:%02X:%02X:%02X:%02X:%02X\n",mac[0][0],mac[0][1],mac[0][2],mac[0][3],mac[0][4],mac[0][5]);
	printf("wan mac: %02X:%02X:%02X:%02X:%02X:%02X\n",mac[1][0],mac[1][1],mac[1][2],mac[1][3],mac[1][4],mac[1][5]);
	if (mac_num >= 3)
		printf("wlan5g mac: %02X:%02X:%02X:%02X:%02X:%02X\n",mac[2][0],mac[2][1],mac[2][2],mac[2][3],mac[2][4],mac[2][5]);
	for(i=3;i<=mac_num;i++){
		if(i==4) printf("BLUETOOTH mac: %02X:%02X:%02X:%02X:%02X:%02X\n",mac[3][0],mac[3][1],mac[3][2],mac[3][3],mac[3][4],mac[3][5]);
		if(i==5) printf("SFP+ mac: %02X:%02X:%02X:%02X:%02X:%02X\n",mac[4][0],mac[4][1],mac[4][2],mac[4][3],mac[4][4],mac[4][5]);
		if(i==6) printf("WLAN11AD mac: %02X:%02X:%02X:%02X:%02X:%02X\n",mac[5][0],mac[5][1],mac[5][2],mac[5][3],mac[5][4],mac[5][5]);
	}

	printf("WPSPIN code: ");
	if(mac_num >3) mac_num=3 ; 
	offset = 6*mac_num + WPSPIN_LENGTH;
	for (i = 6*mac_num; i < offset; ++i)
		printf("%c",sectorBuff[i]);
	printf("\nSerial Number: ");
	offset += SERIAL_NUMBER_LENGTH;
	for (; i < offset; ++i)
		printf("%c",sectorBuff[i]);
	printf("\nSSID: ");
	offset = offset + REGION_NUMBER_LENGTH + BOARD_HW_ID_LENGTH + BOARD_MODEL_ID_LENGTH + BOARD_SSID_LENGTH;
	i = i + REGION_NUMBER_LENGTH + BOARD_HW_ID_LENGTH + BOARD_MODEL_ID_LENGTH;
	for (; i < offset; ++i)
		printf("%c",sectorBuff[i]);
	printf("\nPASSPHRASE: ");
	offset += BOARD_PASSPHRASE_LENGTH;
	for (; i < offset; ++i)
		printf("%c",sectorBuff[i]);
	printf("\n\n");

	set_board_data(0, length, sectorBuff);

	return 0;
}

U_BOOT_CMD(
	board_parameters_set, 11, 0, do_board_parameters_set,
	"Set WPS PIN code, Serial number, SSID, Passphrase, MAC address",
	"<WPS Pin> <SN> <SSID> <PASSPHRASE> <lan address> <wan address> [optional: <wlan5g address>]\n"
	"[optional: bluetooth address] [optional: SFP+ address] [optional: 11AD address]\n"
	"          <WPS Pin> (8 digits)\n"
	"          <SN> Serial number (13 digits)\n"
	"          <SSID> SSID (max 32 digits)\n"
	"          <PASSPHRASE> Passphrase (max 64 digits)\n"
	"          <[lan|wan|wlan5g] address> should be the format as 00:03:7F:EF:77:87\n"
	" For instance: board_parameters_set 12345678 1ML1747D0000B GAEGTEN 1234567890 00:03:7F:EF:77:87 00:03:33:44:66:FE 00:03:77:66:77:88"
);

int do_board_parameters_show(cmd_tbl_t * cmdtp, int flag, int argc,
                             char * const argv[])
{
	unsigned char sectorBuff[CONFIG_SYS_FLASH_SECTOR_SIZE];
	int i, end;

	get_board_data(0, CONFIG_SYS_FLASH_SECTOR_SIZE, sectorBuff);

	printf("WPSPIN code: ");
	end = WPSPIN_OFFSET + WPSPIN_LENGTH;
	for (i = WPSPIN_OFFSET; i < end; ++i)
		printf("%c",sectorBuff[i]);

	printf("\nSerial Number: ");
	end = SERIAL_NUMBER_OFFSET + SERIAL_NUMBER_LENGTH;
	for (i = SERIAL_NUMBER_OFFSET; i < end; ++i)
		printf("%c",sectorBuff[i]);

	printf("\nSSID: ");
	end = BOARD_SSID_OFFSET + BOARD_SSID_LENGTH;
	for (i = BOARD_SSID_OFFSET; i < end; ++i)
		printf("%c",sectorBuff[i]);

	printf("\nPASSPHRASE: ");
	end = BOARD_PASSPHRASE_OFFSET + BOARD_PASSPHRASE_LENGTH;
	for (i = BOARD_PASSPHRASE_OFFSET; i < end; ++i)
		printf("%c",sectorBuff[i]);
	printf("\n");

	i = LAN_MAC_OFFSET;
	printf("lan mac: %02x:%02x:%02x:%02x:%02x:%02x\n",
			sectorBuff[i], sectorBuff[i+1], sectorBuff[i+2],
			sectorBuff[i+3], sectorBuff[i+4], sectorBuff[i+5]);

	i = WAN_MAC_OFFSET;
	printf("wan mac: %02x:%02x:%02x:%02x:%02x:%02x\n",
			sectorBuff[i], sectorBuff[i+1], sectorBuff[i+2],
			sectorBuff[i+3], sectorBuff[i+4], sectorBuff[i+5]);

#if defined WLAN_MAC_OFFSET
	i = WLAN_MAC_OFFSET;
	printf("wlan5g mac: %02x:%02x:%02x:%02x:%02x:%02x\n",
			sectorBuff[i], sectorBuff[i+1], sectorBuff[i+2],
			sectorBuff[i+3], sectorBuff[i+4], sectorBuff[i+5]);
#endif

#if defined BLUETOOTH_MAC_OFFSET
	i = BLUETOOTH_MAC_OFFSET;
	printf("BLUETOOTH mac: %02x:%02x:%02x:%02x:%02x:%02x\n",
			sectorBuff[i], sectorBuff[i+1], sectorBuff[i+2],
			sectorBuff[i+3], sectorBuff[i+4], sectorBuff[i+5]);
#endif

#if defined SFP_MAC_OFFSET
	i = SFP_MAC_OFFSET;
	printf("SFP+ mac: %02x:%02x:%02x:%02x:%02x:%02x\n",
			sectorBuff[i], sectorBuff[i+1], sectorBuff[i+2],
			sectorBuff[i+3], sectorBuff[i+4], sectorBuff[i+5]);
#endif

#if defined WLAN11AD_MAC_OFFSET
	i = WLAN11AD_MAC_OFFSET;
	printf("WLAN11AD mac: %02x:%02x:%02x:%02x:%02x:%02x\n",
			sectorBuff[i], sectorBuff[i+1], sectorBuff[i+2],
			sectorBuff[i+3], sectorBuff[i+4], sectorBuff[i+5]);
#endif
	return 0;
}

U_BOOT_CMD(
	board_parameters_show, 1, 0, do_board_parameters_show,
	"Show WPS PIN code, Serial number, SSID, Passphrase, MAC address.",
	"\n"
	"- show <WPS Pin> <SN> <SSID> <PASSPHRASE> <lan address> <wan address>\n"
	"   [optional: <wlan5g address>] [optional: bluetooth address] [optional: SFP+ address] [optional: 11AD address]\n"
);
#endif

#if defined(CONFIG_QUADRUPLE_MAC_ADDRESS)
int do_board_parameters_set(cmd_tbl_t *cmdtp, int flag, int argc,
                            char * const argv[])
{
	char sectorBuff[6 * 4 +
         WPSPIN_LENGTH + SERIAL_NUMBER_LENGTH +
         REGION_NUMBER_LENGTH + BOARD_HW_ID_LENGTH +
         BOARD_MODEL_ID_LENGTH + BOARD_SSID_LENGTH +
         BOARD_PASSPHRASE_LENGTH

#if defined(SFP_MAC_OFFSET)
         +SFP_MAC_LENGTH
#endif

#if defined(WLAN11AD_MAC_OFFSET)
	+WLAN11AD_MAC_LENGTH
#endif
	];

	u8 mac[7][6] = {
	    {255, 255, 255, 255, 255, 255},
                    {255, 255, 255, 255, 255, 255},
                    {255, 255, 255, 255, 255, 255},
                    {255, 255, 255, 255, 255, 255},
                    {255, 255, 255, 255, 255, 255},
                    {255, 255, 255, 255, 255, 255},
                    {255, 255, 255, 255, 255, 255}
                 };


	char wpspin[WPSPIN_LENGTH] = {0};
	char sn[SERIAL_NUMBER_LENGTH] = {0};
	u8 board_ssid[BOARD_SSID_LENGTH + 1];
	u8 board_passphrase[BOARD_PASSPHRASE_LENGTH + 1];
	int wps_len = 0;
	int sn_len = 0;
	int board_passphrase_len = 0;
	int board_ssid_len = 0;
	int offset, i=0, j=0, k=0, val=0, sum=0, length, mac_num=2, m=0;

	if (argc > 12 || argc < 7) {
		printf("%s\n",cmdtp->usage);
		return 1;
	}
	if (argc >= 8)
		mac_num += argc -7 ;
	printf("mac_num:%d\n",mac_num);

	length = 6 * mac_num + WPSPIN_LENGTH + SERIAL_NUMBER_LENGTH +
					REGION_NUMBER_LENGTH + BOARD_HW_ID_LENGTH +
					BOARD_MODEL_ID_LENGTH + BOARD_SSID_LENGTH +
					BOARD_PASSPHRASE_LENGTH ;

	/* Check WPS length */
	if ((wps_len = strlen(argv[1])) > WPSPIN_LENGTH)
	{
		printf ("the length of wpspin can't > %d\n", WPSPIN_LENGTH);
		return 1;
	}

	/* Check serial number */
	sn_len = strlen(argv[2]);   /*check the SN's length*/
	if (sn_len != SERIAL_NUMBER_LENGTH)
	{
		printf ("SN's len is wrong,it's lenth is %d\n ", SERIAL_NUMBER_LENGTH);
		return 1;
	}

	strncpy(sn, argv[2], SERIAL_NUMBER_LENGTH);
	for (i = 0; i < SERIAL_NUMBER_LENGTH; ++i)/*check seria naumber is 0~9 or A~Z*/
	{
		if (!(((sn[i]>=0x30) && (sn[i]<=0x39)) || ((sn[i]>=0x41) && (sn[i]<=0x5a))))    /*sn is 0~9 or A~Z*/
		{
			puts ("the SN only is 0~9 or A~Z\n");
			break;
		}
	}

	if (i < SERIAL_NUMBER_LENGTH)       /*because SN is not 0~9 or A~Z*/
		return 1;

	/* Check SSID length */
	if ((board_ssid_len = strlen(argv[3])) > BOARD_SSID_LENGTH)
	{
		printf ("the length of SSID can't > %d\n", BOARD_SSID_LENGTH);
		return 1;
	}

	/* Check Passphrase length */
	if ((board_passphrase_len = strlen(argv[4])) > BOARD_PASSPHRASE_LENGTH)
	{
		printf ("the length of PASSPHRASE can't > %d\n", BOARD_PASSPHRASE_LENGTH);
		return 1;
	}

	/* check MAC address */
	for (k = 5; k < 5 + mac_num; ++k)
	{
		sum = 0 , val = 0;
		i = 0; j = 0;
		while (argv[k][i])
		{
			if (':' == argv[k][i])
			{
				mac[m][j++] = val;
				i++;
				sum += val;
				val = 0;
				continue;
			}
			if ((argv[k][i] >= '0') && (argv[k][i] <= '9'))
				val = val*16 + (argv[k][i] - '0');
			else if ((argv[k][i] >='a') && (argv[k][i] <= 'f'))
				val = val*16 + (argv[k][i] - 'a') + 10;
			else if ((argv[k][i] >= 'A') && (argv[k][i] <= 'F'))
				val = val*16 + (argv[k][i] - 'A') + 10;
			else
			{
				printf("The %d MAC address is incorrect\n",k);
				printf("The MAC address can not be all 0x00 or all 0xFF\n");
				return 1;
			}
			i++;
		}
		mac[m][j] = val;
		sum += val;
		m++;
		if (j != 5  || 0 == sum || 1530 == sum)
		{
			printf("The %d MAC address is incorrect\n",k);
			printf("The MAC address can not be all 0x00 or all 0xFF\n");
			return 1;
		}
	}


	/* Copy new settings to buffer */
	get_board_data(0, length, sectorBuff);
	if(mac_num <= 4)
		memcpy(sectorBuff, mac, 6 * mac_num);
	else{
	        memcpy(sectorBuff, mac, 6 * 4);
		memcpy(sectorBuff + BLUETOOTH_MAC_OFFSET, mac[4], 6 * (mac_num-4) );
	}
	memset(wpspin, 0, sizeof(wpspin));
	memcpy(wpspin, argv[1], wps_len);
	memcpy(sectorBuff + WPSPIN_OFFSET, wpspin, WPSPIN_LENGTH);
	memcpy(sectorBuff + SERIAL_NUMBER_OFFSET, sn, SERIAL_NUMBER_LENGTH);
	memset(board_ssid, 0, sizeof(board_ssid));
	memcpy(board_ssid, argv[3], board_ssid_len);
	memcpy(sectorBuff + BOARD_SSID_OFFSET, board_ssid, BOARD_SSID_LENGTH);
	memset(board_passphrase, 0, sizeof(board_passphrase));
	memcpy(board_passphrase, argv[4], board_passphrase_len);
	memcpy(sectorBuff + BOARD_PASSPHRASE_OFFSET, board_passphrase, BOARD_PASSPHRASE_LENGTH);

	printf("Burn the following parameters into ART block.\n");
	printf("lan mac: %02X:%02X:%02X:%02X:%02X:%02X\n",mac[0][0],mac[0][1],mac[0][2],mac[0][3],mac[0][4],mac[0][5]);
	printf("wan mac: %02X:%02X:%02X:%02X:%02X:%02X\n",mac[1][0],mac[1][1],mac[1][2],mac[1][3],mac[1][4],mac[1][5]);
	for(i=2;i < mac_num;i++){
		if(i==2) printf("wlan5g mac: %02X:%02X:%02X:%02X:%02X:%02X\n",mac[i][0],mac[i][1],mac[i][2],mac[i][3],mac[i][4],mac[i][5]);
		if(i==3) printf("BLUETOOTH mac: %02X:%02X:%02X:%02X:%02X:%02X\n",mac[i][0],mac[i][1],mac[i][2],mac[i][3],mac[i][4],mac[i][5]);
		if(i==4) printf("wlan2.4g mac: %02X:%02X:%02X:%02X:%02X:%02X\n",mac[i][0],mac[i][1],mac[i][2],mac[i][3],mac[i][4],mac[i][5]);
		if(i==5) printf("SFP+ mac: %02X:%02X:%02X:%02X:%02X:%02X\n",mac[i][0],mac[i][1],mac[i][2],mac[i][3],mac[i][4],mac[i][5]);
		if(i==6) printf("WLAN11AD mac: %02X:%02X:%02X:%02X:%02X:%02X\n",mac[i][0],mac[i][1],mac[i][2],mac[i][3],mac[i][4],mac[i][5]);
	}

	mac_num=4 ;
	printf("WPSPIN code: ");
	offset = 6*mac_num + WPSPIN_LENGTH;
	for (i = 6*mac_num; i < offset; ++i)
		printf("%c",sectorBuff[i]);
	printf("\nSerial Number: ");
	offset += SERIAL_NUMBER_LENGTH;
	for (; i < offset; ++i)
		printf("%c",sectorBuff[i]);
	printf("\nSSID: ");
	offset = offset + REGION_NUMBER_LENGTH + BOARD_HW_ID_LENGTH + BOARD_MODEL_ID_LENGTH + BOARD_SSID_LENGTH;
	i = i + REGION_NUMBER_LENGTH + BOARD_HW_ID_LENGTH + BOARD_MODEL_ID_LENGTH;
	for (; i < offset; ++i)
		printf("%c",sectorBuff[i]);
	printf("\nPASSPHRASE: ");
	offset += BOARD_PASSPHRASE_LENGTH;
	for (; i < offset; ++i)
		printf("%c",sectorBuff[i]);
	printf("\n\n");

	set_board_data(0, length, sectorBuff);

	return 0;
}

U_BOOT_CMD(
	board_parameters_set, 12, 0, do_board_parameters_set,
	"Set WPS PIN code, Serial number, SSID, Passphrase, MAC address",
	"<WPS Pin> <SN> <SSID> <PASSPHRASE> <lan address> <wan address> [optional: <wlan5g address>]\n"
	"[optional:<wlan2.4g address>] [optional: bluetooth address] [optional: SFP+ address] [optional: 11AD address]\n"
	"          <WPS Pin> (8 digits)\n"
	"          <SN> Serial number (13 digits)\n"
	"          <SSID> SSID (max 32 digits)\n"
	"          <PASSPHRASE> Passphrase (max 64 digits)\n"
	"          <[lan|wan|wlan5g|wlan2.4g] address> should be the format as 00:03:7F:EF:77:87\n"
	" For instance: board_parameters_set 12345678 1ML1747D0000B GAEGTEN 1234567890 00:03:7F:EF:77:87 00:03:33:44:66:FE 00:03:77:66:77:88 00:03:08:23:24:30"
);

int do_board_parameters_show(cmd_tbl_t * cmdtp, int flag, int argc,
                             char * const argv[])
{
	unsigned char sectorBuff[CONFIG_SYS_FLASH_SECTOR_SIZE];
	int i, end;

	get_board_data(0, CONFIG_SYS_FLASH_SECTOR_SIZE, sectorBuff);

	printf("WPSPIN code: ");
	end = WPSPIN_OFFSET + WPSPIN_LENGTH;
	for (i = WPSPIN_OFFSET; i < end; ++i)
		printf("%c",sectorBuff[i]);

	printf("\nSerial Number: ");
	end = SERIAL_NUMBER_OFFSET + SERIAL_NUMBER_LENGTH;
	for (i = SERIAL_NUMBER_OFFSET; i < end; ++i)
		printf("%c",sectorBuff[i]);

	printf("\nSSID: ");
	end = BOARD_SSID_OFFSET + BOARD_SSID_LENGTH;
	for (i = BOARD_SSID_OFFSET; i < end; ++i)
		printf("%c",sectorBuff[i]);

	printf("\nPASSPHRASE: ");
	end = BOARD_PASSPHRASE_OFFSET + BOARD_PASSPHRASE_LENGTH;
	for (i = BOARD_PASSPHRASE_OFFSET; i < end; ++i)
		printf("%c",sectorBuff[i]);
	printf("\n");

	i = LAN_MAC_OFFSET;
	printf("lan mac: %02x:%02x:%02x:%02x:%02x:%02x\n",
			sectorBuff[i], sectorBuff[i+1], sectorBuff[i+2],
			sectorBuff[i+3], sectorBuff[i+4], sectorBuff[i+5]);

	i = WAN_MAC_OFFSET;
	printf("wan mac: %02x:%02x:%02x:%02x:%02x:%02x\n",
			sectorBuff[i], sectorBuff[i+1], sectorBuff[i+2],
			sectorBuff[i+3], sectorBuff[i+4], sectorBuff[i+5]);

#if defined  WLAN_5G_MAC_OFFSET
	i = WLAN_5G_MAC_OFFSET;
#elif defined WLAN_MAC_OFFSET
        i = WLAN_MAC_OFFSET;
	printf("wlan5g mac: %02x:%02x:%02x:%02x:%02x:%02x\n",
			sectorBuff[i], sectorBuff[i+1], sectorBuff[i+2],
			sectorBuff[i+3], sectorBuff[i+4], sectorBuff[i+5]);
#endif

#if defined  WLAN_24G_MAC_OFFSET
        i = WLAN_24G_MAC_OFFSET;
        printf("wlan2.4g mac: %02x:%02x:%02x:%02x:%02x:%02x\n",
                        sectorBuff[i], sectorBuff[i+1], sectorBuff[i+2],
                        sectorBuff[i+3], sectorBuff[i+4], sectorBuff[i+5]);
#endif

#if defined BLUETOOTH_MAC_OFFSET
	i = BLUETOOTH_MAC_OFFSET;
	printf("BLUETOOTH mac: %02x:%02x:%02x:%02x:%02x:%02x\n",
			sectorBuff[i], sectorBuff[i+1], sectorBuff[i+2],
			sectorBuff[i+3], sectorBuff[i+4], sectorBuff[i+5]);
#endif

#if defined SFP_MAC_OFFSET
	i = SFP_MAC_OFFSET;
	printf("SFP+ mac: %02x:%02x:%02x:%02x:%02x:%02x\n",
			sectorBuff[i], sectorBuff[i+1], sectorBuff[i+2],
			sectorBuff[i+3], sectorBuff[i+4], sectorBuff[i+5]);
#endif

#if defined WLAN11AD_MAC_OFFSET
	i = WLAN11AD_MAC_OFFSET;
	printf("WLAN11AD mac: %02x:%02x:%02x:%02x:%02x:%02x\n",
			sectorBuff[i], sectorBuff[i+1], sectorBuff[i+2],
			sectorBuff[i+3], sectorBuff[i+4], sectorBuff[i+5]);
#endif
	return 0;
}

U_BOOT_CMD(
	board_parameters_show, 1, 0, do_board_parameters_show,
	"Show WPS PIN code, Serial number, SSID, Passphrase, MAC address.",
	"\n"
	"- show <WPS Pin> <SN> <SSID> <PASSPHRASE> <lan address> <wan address>\n"
#if defined  WLAN_24G_MAC_OFFSET
	"   [optional: <wlan5g address> <wlan2.4g address>] [optional: bluetooth address] [optional: SFP+ address] [optional: 11AD address]\n"
#else
	"   [optional: <wlan5g address>] [optional: bluetooth address] [optional: SFP+ address] [optional: 11AD address]\n"
#endif
);
#endif

#ifdef CONFIG_QUINTUPLE_MAC_ADDRESS
int do_board_parameters_set(cmd_tbl_t *cmdtp, int flag, int argc,
                            char * const argv[])
{
    char sectorBuff[6 * 5 +
         WPSPIN_LENGTH + SERIAL_NUMBER_LENGTH +
         REGION_NUMBER_LENGTH + BOARD_HW_ID_LENGTH +
         BOARD_MODEL_ID_LENGTH + BOARD_SSID_LENGTH +
         BOARD_BACKHAUL_SSID_LENGTH +
         BOARD_PASSPHRASE_LENGTH +
         BOARD_BACKHAUL_PASSPHRASE_LENGTH +
         BOARD_DATA_LENGTH];
    u8 mac[5][6] = {{255, 255, 255, 255, 255, 255},
                    {255, 255, 255, 255, 255, 255},
                    {255, 255, 255, 255, 255, 255},
                    {255, 255, 255, 255, 255, 255},
                    {255, 255, 255, 255, 255, 255}
                   };
    char wpspin[WPSPIN_LENGTH] = {0};
    char sn[SERIAL_NUMBER_LENGTH] = {0};
    u8 board_ssid[BOARD_SSID_LENGTH + 1];
    u8 board_backhaul_ssid[BOARD_BACKHAUL_SSID_LENGTH + 1];
    u8 board_passphrase[BOARD_PASSPHRASE_LENGTH + 1];
    u8 board_backhaul_passphrase[BOARD_BACKHAUL_PASSPHRASE_LENGTH + 1];
    u8 board_data[BOARD_DATA_LENGTH+1];
    int wps_len = 0;
    int sn_len = 0;
    int board_passphrase_len = 0;
    int board_backhaul_passphrase_len = 0;
    int board_ssid_len = 0;
    int board_backhaul_ssid_len = 0;
    int board_data_len = 0;
    int offset, i=0, j=0, k=0, val=0, sum=0, length, mac_num=2, m=0;

    if (argc > 13 || argc < 12) {
        printf("%s\n",cmdtp->usage);
        return 1;
    }
    if (argc == 13)
        mac_num = 5;
//  printf ("mac_num = %s\n",mac_num);
    length = 6 * mac_num + WPSPIN_LENGTH + SERIAL_NUMBER_LENGTH +
                    REGION_NUMBER_LENGTH + BOARD_HW_ID_LENGTH +
                    BOARD_MODEL_ID_LENGTH + BOARD_SSID_LENGTH +
                    BOARD_BACKHAUL_SSID_LENGTH +
                    BOARD_PASSPHRASE_LENGTH +
                    BOARD_BACKHAUL_PASSPHRASE_LENGTH +
                    BOARD_DATA_LENGTH;

    /* Check WPS length */
    if ((wps_len = strlen(argv[1])) > WPSPIN_LENGTH)
    {
        printf ("the length of wpspin can't > %d\n", WPSPIN_LENGTH);
        return 1;
    }

    /* Check serial number */
    sn_len = strlen(argv[2]);   /*check the SN's length*/
    if (sn_len != SERIAL_NUMBER_LENGTH)
    {
        printf ("SN's len is wrong,it's lenth is %d\n ", SERIAL_NUMBER_LENGTH);
        return 1;
    }

    strncpy(sn, argv[2], SERIAL_NUMBER_LENGTH);
    for (i = 0; i < SERIAL_NUMBER_LENGTH; ++i)/*check seria naumber is 0~9 or A~Z*/
    {
        if (!(((sn[i]>=0x30) && (sn[i]<=0x39)) || ((sn[i]>=0x41) && (sn[i]<=0x5a))))    /*sn is 0~9 or A~Z*/
        {
            puts ("the SN only is 0~9 or A~Z\n");
            break;
        }
    }

    if (i < SERIAL_NUMBER_LENGTH)       /*because SN is not 0~9 or A~Z*/
        return 1;

    /* Check SSID length */
    if ((board_ssid_len = strlen(argv[3])) > BOARD_SSID_LENGTH)
    {
        printf ("the length of SSID can't > %d\n", BOARD_SSID_LENGTH);
        return 1;
    }

    /* Check BACKHAUL SSID length */
    if ((board_backhaul_ssid_len = strlen(argv[4])) > BOARD_BACKHAUL_SSID_LENGTH)
    {
        printf ("the length of SSID can't > %d\n", BOARD_BACKHAUL_SSID_LENGTH);
        return 1;
    }

    /* Check Passphrase length */
    if ((board_passphrase_len = strlen(argv[5])) > BOARD_PASSPHRASE_LENGTH)
    {
        printf ("the length of PASSPHRASE can't > %d\n", BOARD_PASSPHRASE_LENGTH);
        return 1;
    }

    /* Check Backhaul Passphrase length */
    if ((board_backhaul_passphrase_len = strlen(argv[6])) > BOARD_BACKHAUL_PASSPHRASE_LENGTH)
    {
        printf ("the length of PASSPHRASE can't > %d\n", BOARD_BACKHAUL_PASSPHRASE_LENGTH);
        return 1;
    }

    /* Check Board Data length */
    if ((board_data_len = strlen(argv[7])) > BOARD_DATA_LENGTH)
    {
        printf ("the length of Board Data can't > %d\n", BOARD_DATA_LENGTH);
        return 1;
    }

    /* check MAC address */
    for (k = 8; k < 8 + mac_num; ++k)
    {
        sum = 0 , val = 0;
        i = 0; j = 0;
        while (argv[k][i])
        {
            if (':' == argv[k][i])
            {
                mac[m][j++] = val;
                i++;
                sum += val;
                val = 0;
                continue;
            }
            if ((argv[k][i] >= '0') && (argv[k][i] <= '9'))
                val = val*16 + (argv[k][i] - '0');
            else if ((argv[k][i] >='a') && (argv[k][i] <= 'f'))
                val = val*16 + (argv[k][i] - 'a') + 10;
            else if ((argv[k][i] >= 'A') && (argv[k][i] <= 'F'))
                val = val*16 + (argv[k][i] - 'A') + 10;
            else
            {
                printf("The %d MAC address is incorrect\n",k);
                printf("The MAC address can not be all 0x00 or all 0xFF\n");
                return 1;
            }
            i++;
        }
        mac[m][j] = val;
        sum += val;
        m++;
        if (j != 5  || 0 == sum || 1530 == sum)
        {
            printf("The %d MAC address is incorrect\n",k); printf("The MAC address can not be all 0x00 or all 0xFF\n"); return 1; } }

    /* Copy new settings to buffer */
    get_board_data(0, length, sectorBuff);
    memcpy(sectorBuff, mac, 6 * mac_num);

    memset(wpspin, 0, sizeof(wpspin));
    memcpy(wpspin, argv[1], wps_len);
    memcpy(sectorBuff + WPSPIN_OFFSET, wpspin, WPSPIN_LENGTH);

    memcpy(sectorBuff + SERIAL_NUMBER_OFFSET, sn, SERIAL_NUMBER_LENGTH);

    memset(board_ssid, 0, sizeof(board_ssid));
    memcpy(board_ssid, argv[3], board_ssid_len);
    memcpy(sectorBuff + BOARD_SSID_OFFSET, board_ssid, BOARD_SSID_LENGTH);

    memset(board_backhaul_ssid, 0, sizeof(board_backhaul_ssid));
    memcpy(board_backhaul_ssid, argv[4], board_backhaul_ssid_len);
    memcpy(sectorBuff + BOARD_BACKHAUL_SSID_OFFSET, board_backhaul_ssid, BOARD_BACKHAUL_SSID_LENGTH);

    memset(board_passphrase, 0, sizeof(board_passphrase));
    memcpy(board_passphrase, argv[5], board_passphrase_len);
    memcpy(sectorBuff + BOARD_PASSPHRASE_OFFSET, board_passphrase, BOARD_PASSPHRASE_LENGTH);

    memset(board_backhaul_passphrase, 0, sizeof(board_backhaul_passphrase));
    memcpy(board_backhaul_passphrase, argv[6], board_backhaul_passphrase_len);
    memcpy(sectorBuff + BOARD_BACKHAUL_PASSPHRASE_OFFSET, board_backhaul_passphrase, BOARD_BACKHAUL_PASSPHRASE_LENGTH);

    memset(board_data, 0, sizeof(board_data));
    memcpy(board_data, argv[7], board_data_len);
    memcpy(sectorBuff + BOARD_DATA_OFFSET, board_data, BOARD_DATA_LENGTH);

    printf("Burn the following parameters into ART block.\n");
    printf("lan mac: %02X:%02X:%02X:%02X:%02X:%02X\n",mac[0][0],mac[0][1],mac[0][2],mac[0][3],mac[0][4],mac[0][5]);
    printf("wan mac: %02X:%02X:%02X:%02X:%02X:%02X\n",mac[1][0],mac[1][1],mac[1][2],mac[1][3],mac[1][4],mac[1][5]);
    printf("wlan5g mac: %02X:%02X:%02X:%02X:%02X:%02X\n",mac[2][0],mac[2][1],mac[2][2],mac[2][3],mac[2][4],mac[2][5]);
    printf("wlan2nd5g mac: %02X:%02X:%02X:%02X:%02X:%02X\n",mac[3][0],mac[3][1],mac[3][2],mac[3][3],mac[3][4],mac[3][5]);
    printf("bt mac: %02X:%02X:%02X:%02X:%02X:%02X\n",mac[4][0],mac[4][1],mac[4][2],mac[4][3],mac[4][4],mac[4][5]);
    printf("WPSPIN code: ");
    offset = 6*mac_num + WPSPIN_LENGTH;
    for (i = 6*mac_num; i < offset; ++i)
        printf("%c",sectorBuff[i]);
    printf("\nSerial Number: ");
    offset += SERIAL_NUMBER_LENGTH;
    for (; i < offset; ++i)
        printf("%c",sectorBuff[i]);
    printf("\nSSID: ");
    offset = offset + REGION_NUMBER_LENGTH + BOARD_HW_ID_LENGTH + BOARD_MODEL_ID_LENGTH + BOARD_SSID_LENGTH;
    i = i + REGION_NUMBER_LENGTH + BOARD_HW_ID_LENGTH + BOARD_MODEL_ID_LENGTH;
    for (; i < offset; ++i)
        printf("%c",sectorBuff[i]);
    printf("\nBACKHAUL SSID: ");
    offset += BOARD_BACKHAUL_SSID_LENGTH;
    for (; i < offset; ++i)
        printf("%c",sectorBuff[i]);
    printf("\nPASSPHRASE: ");
    offset += BOARD_PASSPHRASE_LENGTH;
    for (; i < offset; ++i)
        printf("%c",sectorBuff[i]);
    printf("\nBACKHAUL PASSPHRASE: ");
    offset += BOARD_BACKHAUL_PASSPHRASE_LENGTH;
    for (; i < offset; ++i)
        printf("%c",sectorBuff[i]);
    printf("\nBOARD DATA: ");
    offset += BOARD_DATA_LENGTH;
    for (; i < offset; ++i)
        printf("%c",sectorBuff[i]);
    printf("\n\n");

    set_board_data(0, length, sectorBuff);

    return 0;
}

U_BOOT_CMD(
    board_parameters_set, 13, 0, do_board_parameters_set,
    "Set WPS PIN code, Serial number, SSID, Passphrase, Board data, MAC address",
    "<WPS Pin> <SN> <SSID> <BACKHAUL SSID> <PASSPHRASE> <BACKHAUL PASSPHRASE> <BOARD DATA> <lan address> <wan address> <wlan5g address> <wlan2nd5g address> <bt address>\n"
    "          <WPS Pin> (8 digits)\n"
    "          <SN> Serial number (13 digits)\n"
    "          <SSID> SSID (max 32 digits)\n"
    "          <BACKHAUL SSID> SSID (max 32 digits)\n"
    "          <PASSPHRASE> Passphrase (max 64 digits)\n"
    "          <<BACKHAUL PASSPHRASE>> Passphrase (max 64 digits)\n"
    "          <<BOARD DATA>> Board data (max 4 digits)\n"
    "          <[lan|wan|wlan5g|wlan2nd5g|bt] address> should be the format as 00:03:7F:EF:77:87\n"
    " For instance: board_parameters_set 12345678 1ML1747D0000B NETGEAR NETGEARS 1234567890 2345678901 1101010100000A00 00:03:7F:EF:77:87 00:03:33:44:66:FE 00:03:77:66:77:88 00:09:11:23:30:40 00:02:15:16:24:29"
);

int do_board_parameters_show(cmd_tbl_t * cmdtp, int flag, int argc,
                             char * const argv[])
{
    unsigned char sectorBuff[CONFIG_SYS_FLASH_SECTOR_SIZE];
    int i, end;

    get_board_data(0, CONFIG_SYS_FLASH_SECTOR_SIZE, sectorBuff);

    printf("WPSPIN code: ");
    end = WPSPIN_OFFSET + WPSPIN_LENGTH;
    for (i = WPSPIN_OFFSET; i < end; ++i)
        printf("%c",sectorBuff[i]);

    printf("\nSerial Number: ");
    end = SERIAL_NUMBER_OFFSET + SERIAL_NUMBER_LENGTH;
    for (i = SERIAL_NUMBER_OFFSET; i < end; ++i)
        printf("%c",sectorBuff[i]);

    printf("\nSSID: ");
    end = BOARD_SSID_OFFSET + BOARD_SSID_LENGTH;
    for (i = BOARD_SSID_OFFSET; i < end; ++i)
        printf("%c",sectorBuff[i]);

    printf("\nBACKHAUL SSID: ");
    end = BOARD_BACKHAUL_SSID_OFFSET + BOARD_BACKHAUL_SSID_LENGTH;
    for (i = BOARD_BACKHAUL_SSID_OFFSET; i < end; ++i)
        printf("%c",sectorBuff[i]);

    printf("\nPASSPHRASE: ");
    end = BOARD_PASSPHRASE_OFFSET + BOARD_PASSPHRASE_LENGTH;
    for (i = BOARD_PASSPHRASE_OFFSET; i < end; ++i)
        printf("%c",sectorBuff[i]);

    printf("\nBACKHAUL PASSPHRASE: ");
    end = BOARD_BACKHAUL_PASSPHRASE_OFFSET + BOARD_BACKHAUL_PASSPHRASE_LENGTH;
    for (i = BOARD_BACKHAUL_PASSPHRASE_OFFSET; i < end; ++i)
        printf("%c",sectorBuff[i]);

    printf("\nBOARD DATA: ");
    end = BOARD_DATA_OFFSET + BOARD_DATA_LENGTH;
    for (i = BOARD_DATA_OFFSET; i < end; ++i)
        printf("%c",sectorBuff[i]);
    printf("\n");

    i = LAN_MAC_OFFSET;
    printf("lan mac: %02x:%02x:%02x:%02x:%02x:%02x\n",
            sectorBuff[i], sectorBuff[i+1], sectorBuff[i+2],
            sectorBuff[i+3], sectorBuff[i+4], sectorBuff[i+5]);

    i = WAN_MAC_OFFSET;
    printf("wan mac: %02x:%02x:%02x:%02x:%02x:%02x\n",
            sectorBuff[i], sectorBuff[i+1], sectorBuff[i+2],
            sectorBuff[i+3], sectorBuff[i+4], sectorBuff[i+5]);

#if defined WLAN_MAC_OFFSET
    i = WLAN_MAC_OFFSET;
    printf("wlan5g mac: %02x:%02x:%02x:%02x:%02x:%02x\n",
            sectorBuff[i], sectorBuff[i+1], sectorBuff[i+2],
            sectorBuff[i+3], sectorBuff[i+4], sectorBuff[i+5]);
#endif

    i = WLAN_2nd5G_MAC_OFFSET;
    printf("wlan_2nd5g mac: %02x:%02x:%02x:%02x:%02x:%02x\n",
            sectorBuff[i], sectorBuff[i+1], sectorBuff[i+2],
            sectorBuff[i+3], sectorBuff[i+4], sectorBuff[i+5]);

    i = BT_MAC_OFFSET;
    printf("bt mac: %02x:%02x:%02x:%02x:%02x:%02x\n",
            sectorBuff[i], sectorBuff[i+1], sectorBuff[i+2],
            sectorBuff[i+3], sectorBuff[i+4], sectorBuff[i+5]);

    return 0;
}

U_BOOT_CMD(
    board_parameters_show, 1, 0, do_board_parameters_show,
    "Show WPS PIN code, Serial number, SSID, Passphrase, MAC address.",
    "\n"
    "- show <WPS Pin> <SN> <SSID> <PASSPHRASE> <lan address> <wan address>\n"
    "           [optional: <wlan5g address>]\n"
);

#endif /* CONFIG_QUINTUPLE_MAC_ADDRESS */

#endif  /* CONFIG_CMD_BOARD_PARAMETERS */

#endif /* BOARDCAL */

#ifdef CONFIG_DNI_BOARDDATA2
void get_board_data_2(int offset, int len, u8* buf)
{
	size_t read_size = CONFIG_SYS_FLASH_SECTOR_SIZE;
	unsigned char buffer[CONFIG_SYS_FLASH_SECTOR_SIZE];
	int ret = 0;
	loff_t current_offset = (loff_t)BOARDDATA2_ADDR;

        while (nand_block_isbad(&nand_info[0], current_offset)) {
                printf("Skipping bad block 0x%08llx\n", current_offset);
                current_offset += CONFIG_SYS_FLASH_SECTOR_SIZE;
        }
        ret = nand_read_skip_bad(&nand_info[0], current_offset, &read_size, NULL, CONFIG_SYS_FLASH_SECTOR_SIZE, (u_char *)buffer);
	printf(" %zu bytes read: %s\n", read_size,
	       ret ? "ERROR" : "OK");

	memcpy(buf, (void *)(buffer + offset), len);
}

/*function set_board_data_2()
 *description:
 *write data to the flash.
 * return value: 0 (success), 1 (fail)
 */
int set_board_data_2(int offset, int len, u8 *buf)
{
	char sectorBuff[CONFIG_SYS_FLASH_SECTOR_SIZE];
	size_t read_size = CONFIG_SYS_FLASH_SECTOR_SIZE;
	nand_erase_options_t nand_erase_options;
	size_t write_size = CONFIG_SYS_FLASH_SECTOR_SIZE;
	int ret = 0;
	loff_t current_offset = (loff_t)BOARDDATA2_ADDR;

        while (nand_block_isbad(&nand_info[0], current_offset)) {
                printf("Skipping bad block 0x%08llx\n", current_offset);
                current_offset += CONFIG_SYS_FLASH_SECTOR_SIZE;
        }
        ret = nand_read_skip_bad(&nand_info[0], current_offset, &read_size, NULL, CONFIG_SYS_FLASH_SECTOR_SIZE, (u_char *)sectorBuff);
	printf(" %zu bytes read: %s\n", read_size,
	       ret ? "ERROR" : "OK");
	memcpy(sectorBuff + offset, buf, len);
	update_data(sectorBuff, CONFIG_SYS_FLASH_SECTOR_SIZE,
			BOARDDATA2_ADDR, BOARDDATA2_LEN, 0, 0);
	return 0;
}


#if defined(BOARD_DATA_OFFSET) && defined(BOARD_DATA_LENGTH)

int do_board_data_set(cmd_tbl_t *cmdtp, int flag, int argc,
                            char * const argv[])
{
	u8 board_data[BOARD_DATA_LENGTH + 1];
	int board_data_len = 0;

	if (argc != 2) {
		printf("%s\n",cmdtp->usage);
		return 1;
	}
	if ((board_data_len = strlen(argv[1])) > BOARD_DATA_LENGTH) {
		printf ("the length of BOARD DATA can't > %d\n", BOARD_DATA_LENGTH);
		return 1;
	}

	memset(board_data, 0, sizeof(board_data));
	memcpy(board_data, argv[1], board_data_len);

	printf("Burn BOARD DATA (= %s) into ART block\n", board_data);
#if defined(CONFIG_HW29766097P0P512P1024P2X2P2X2P2X2)
	set_board_data(BOARD_DATA_OFFSET, BOARD_DATA_LENGTH, board_data);
#endif
#if defined(CONFIG_HW29765589P0P512P1024P4X4P8X8)
	set_board_data_2(BOARD_DATA_OFFSET, BOARD_DATA_LENGTH, board_data);
#endif
	puts("Done.\n");
	return 0;
}

U_BOOT_CMD(
	board_data_set, 2, 0, do_board_data_set,
	"Set board data on board",
	"XXXXXX\n"
	" For instance: board_data_set 01"
);

void get_board_data_in_art(u8* buf) /* sizeof(buf) must > BOARD_DATA_LENGTH */
{
#if defined(CONFIG_HW29765589P0P512P1024P4X4P8X8) 
	get_board_data_2(BOARD_DATA_OFFSET, BOARD_DATA_LENGTH, buf);
#endif
#if defined(CONFIG_HW29766097P0P512P1024P2X2P2X2P2X2)
	get_board_data(BOARD_DATA_OFFSET, BOARD_DATA_LENGTH, buf);
#endif
}

void get_board_boot_partition_in_art(u8* buf) /* sizeof(buf) must > BOARD_BOOT_PARTITION_LENGTH */
{
	get_board_data_2(BOARD_BOOT_PARTITION_OFFSET, BOARD_BOOT_PARTITION_LENGTH, buf);
}

int do_board_data_show(cmd_tbl_t *cmdtp, int flag, int argc,
                             char * const argv[])
{
	u8 board_data[BOARD_DATA_LENGTH + 1];
	int i = 0;

	memset(board_data, 0, sizeof(board_data));
	get_board_data_in_art(board_data);
	printf("board_data: ");
	for (i=0;i<BOARD_DATA_LENGTH;i++)
		printf("%c", board_data[i]);
	printf("\n");
	return 0;
}

U_BOOT_CMD(
	board_data_show, 1, 0, do_board_data_show,
	"Show board_data",
	"\n"
	" For instance: board_data_show"
);
#endif /* BOARD_DATA_OFFSET BOARD_DATA_LENGTH */


#if defined(BOARD_BOOT_PARTITION_OFFSET) && defined(BOARD_BOOT_PARTITION_LENGTH)

int do_boot_partition_set(cmd_tbl_t *cmdtp, int flag, int argc,
                            char * const argv[])
{
	u8 board_data[BOARD_BOOT_PARTITION_LENGTH + 1];
	char s[]="1";
	char t[]="2";

	if (argc != 2) {
		printf("%s\n",cmdtp->usage);
		return 1;
	}

	memset(board_data, 0, sizeof(board_data));
	get_board_boot_partition_in_art(board_data);
	int test = strcmp(argv[1],s);
	int test2 = strcmp(argv[1],t);

	if (test == 0) {
//		printf("Set to 1 \n");
		board_data[0] = '1';
//		board_data[1] = '1';
	}
	if (test2 == 0) {
//		printf("Set to 2 \n");
		board_data[0] = '2';
//		board_data[1] = '2';
	}

	printf("Burn BOOT PARTITION DATA (= %s) into ART block\n", board_data);
	set_board_data_2(BOARD_BOOT_PARTITION_OFFSET, BOARD_BOOT_PARTITION_LENGTH, board_data);
	puts("Done.\n");
	return 0;
}

U_BOOT_CMD(
	boot_partition_set, 2, 0, do_boot_partition_set,
	"Set boot partition data on board",
	"\n"
	" For instance: boot_partition_set 1 - The value is 1 for first FW partition"
	" For instance: boot_partition_set 2 - The value is 2 for second FW partition"
);

int do_boot_partition_show(cmd_tbl_t *cmdtp, int flag, int argc,
                             char * const argv[])
{
	u8 board_data[BOARD_BOOT_PARTITION_LENGTH + 1];

	memset(board_data, 0, sizeof(board_data));
	get_board_boot_partition_in_art(board_data);
	printf("boot partition : %c\n", board_data[0]);
	if (board_data[0] == '1'){
//		printf("return 1");
		return 1;
	}
	if (board_data[0] == '2'){
//		printf("return 2");
		return 2;
	}
	return 0;
}

U_BOOT_CMD(
	boot_partition_show, 1, 0, do_boot_partition_show,
	"Show boot partition",
	"\n"
	" For instance: boot_partition_show"
);
int boot_partition()
{
	u8 board_data[BOARD_BOOT_PARTITION_LENGTH + 1];
	memset(board_data, 0, sizeof(board_data));
	get_board_boot_partition_in_art(board_data);
	printf("boot partition : %c\n", board_data[0]);
	if (board_data[0] == '1'){
		return 1;
	}
	if (board_data[0] == '2'){
		return 2;
	}
	else {
		char runcmd[256];
		snprintf(runcmd, sizeof(runcmd), "boot_partition_set 1");
		run_command(runcmd, 0);
		return 1;
	}
//	return 0;
}
#endif

#endif /* BOARDDATA2_ADDR */

#if defined(NMRP_FLAG)
#if defined(NMRP_FLAG_OFFSET) && defined(NMRP_FLAG_LENGTH)

int do_nmrp_flag_set(cmd_tbl_t *cmdtp, int flag, int argc,
                            char * const argv[])
{
       u8 nmrp_flag[NMRP_FLAG_LENGTH + 1];
       int nmrp_flag_len = 0;

       if (argc != 2) {
               printf("%s\n",cmdtp->usage);
               return 1;
       }
       if ((nmrp_flag_len = strlen(argv[1])) > NMRP_FLAG_LENGTH) {
               return 1;
       }

       memset(nmrp_flag, 0, sizeof(nmrp_flag));
       memcpy(nmrp_flag, argv[1], nmrp_flag_len);

       printf("Burn nmrp_flag (= %s) into ART block\n", nmrp_flag);
#ifdef CONFIG_DNI_BOARDDATA2
       set_board_data_2(NMRP_FLAG_OFFSET, NMRP_FLAG_LENGTH, nmrp_flag);
#else
       set_board_data(NMRP_FLAG_OFFSET, NMRP_FLAG_LENGTH, nmrp_flag);
#endif
       puts("Done.\n");
       return 0;

}

U_BOOT_CMD(
       nmrp_flag_set, 2, 0, do_nmrp_flag_set,
       "Set nmrp flag on board",
       "\n"
       " For instance: nmrp_flag_set 1 - The value is 1 to turn on NMRP"
       " For instance: nmrp_flag_set 2 - The value is 2 to turn off NMRP"
);

void get_nmrp_flag(u8* buf) /* sizeof(buf) must > BOARD_SSID_LENGTH */
{
#ifdef CONFIG_DNI_BOARDDATA2
       get_board_data_2(NMRP_FLAG_OFFSET, NMRP_FLAG_LENGTH, buf);
#else
       get_board_data(NMRP_FLAG_OFFSET, NMRP_FLAG_LENGTH, buf);
#endif
}

int do_nmrp_flag_show(cmd_tbl_t *cmdtp, int flag, int argc,
                       char * const argv[])
{
       u8 nmrp_flag[NMRP_FLAG_LENGTH + 1];

       memset(nmrp_flag, 0, sizeof(nmrp_flag));
       get_nmrp_flag(nmrp_flag);
       printf("nmrp_flag : %s\n", nmrp_flag);
       return 0;
}

U_BOOT_CMD(
       nmrp_flag_show, 1, 0, do_nmrp_flag_show,
       "Show nmrp flag",
       "\n"
       " For instance: nmrp_flag_show"
);

int nmrp_flag()
{
       u8 nmrp_flag[NMRP_FLAG_LENGTH + 1];
       memset(nmrp_flag, 0, sizeof(nmrp_flag));
       get_nmrp_flag(nmrp_flag);
       printf("nmrp flag : %c\n", nmrp_flag[0]);
       if (nmrp_flag[0] == '1'){
               return 1;
       }
       else {
               return 0;
       }
}

int do_bootnmrp(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
       int nmrpflag = 0;
       char runcmd[256];

       printf("Read NMRPflag from BOARD DATA...\n");
       nmrpflag = nmrp_flag();

//     printf("NMRPflag = %d\n",nmrpflag);

       if (nmrpflag == 1)
       {
               printf("NMRP Checking...\n");
               snprintf(runcmd, sizeof(runcmd), "nmrp");
               run_command(runcmd, 0);
       }
       else
       {
               printf("NMRP off\n");
       }
       return 0;
}

U_BOOT_CMD(
       bootnmrp,   1,  0, do_bootnmrp,
       "Check NMRP should execute or not",
       " <offset>\n"
       "    - Check the NMRP should execute or not.\n"
);

#endif
#endif

#ifdef CHECK_DNI_FIRMWARE_INTEGRITY
/**
 * dni_nand_read_eb_manage_bad:
 *
 * Read a erase block (eb) from NAND flash.
 *
 * Blocks that have been marked bad are skipped and the next block is read
 * instead as long as the image is short enough to fit even after skipping the
 * bad blocks.
 *
 * BECAREFUL! If a block is found unable to be corrected by data in OOB, the
 * block is marked as bad block if marking bad block function is implemented
 * in NAND flash driver. **If you do not want to try experimental
 * marking-bad-block function, use nand_read_skip_bad() in
 * drivers/mtd/nand/nand_util.c instead.**
 *
 * @param nand       NAND device
 * @param block_num  Block number to be read. Block number of the first block
 *                   on NAND flash is 0.
 *                   When leaving this function, it equals to block number
 *                   which is truly read after bad blocks are skipped.
 * @param buffer     buffer to write to
 * @return 0 in case of success
 */
int dni_nand_read_eb_manage_bad(nand_info_t *nand, ulong *block_num,
                                u_char *buffer)
{
	int rval;
	uint64_t block_offset = (*block_num) * CONFIG_SYS_FLASH_SECTOR_SIZE;
	size_t read_length = CONFIG_SYS_FLASH_SECTOR_SIZE;

	while (block_offset < nand->size &&
	       nand_block_isbad(nand, block_offset)) {
		printf("Skipping bad block 0x%08llx\n", block_offset);

		block_offset += CONFIG_SYS_FLASH_SECTOR_SIZE;
		(*block_num)++;
	}

	if (block_offset >= nand->size) {
		printf("Attempt to read outside the flash area\n");
		return -EINVAL;
	}
#ifdef CONFIG_HW29765235P0P512P1024P4X4P4X4
	char runcmd[256];
	snprintf(runcmd, sizeof(runcmd), "nand set_partition_offset 0x%lx", block_offset);
	printf("runcommand %s\n" , runcmd);
	if (run_command(runcmd, 0) != CMD_RET_SUCCESS) {
		printf ("nand set_partition_offset error\n");
	}
#endif
	rval = nand_read_skip_bad(nand, block_offset, &read_length, NULL, 0x20000, (u_char *)buffer);
	/* Set partition_offset back to $nand_pt_addr_al_boot after */
	/* calling nand_read_skip_bad function                      */
#ifdef CONFIG_HW29765235P0P512P1024P4X4P4X4
	snprintf(runcmd, sizeof(runcmd), "nand set_partition_offset 0x0");
	printf("runcommand %s\n" , runcmd);
	if (run_command(runcmd, 0) != CMD_RET_SUCCESS) {
		printf ("nand set_partition_offset error\n");
	}
#endif
	if (rval == -EBADMSG && mtd_block_markbad != NULL) {
		printf("Block 0x%llx is marked as bad block!!\n",
		       block_offset);
		mtd_block_markbad(nand, block_offset);
		do_reset(NULL, 0, 0, NULL);

	} else if (rval && rval != -EUCLEAN) {
		printf("NAND read from block %llx failed %d\n", block_offset,
		       rval);
		return rval;
	}
	return 0;
}

int do_loadn_dniimg(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	int dev;
	ulong offset, addr, kernel_partition_size;
	ulong addr_end;
	ulong block_num;
	image_header_t *hdr;
	nand_info_t *nand;

	if (argc != 4) {
		printf("Usage:\n%s\n", cmdtp->usage);
		return 1;
	}
	dev = simple_strtoul(argv[1], NULL, 16);
	offset = simple_strtoul(argv[2], NULL, 16);
	addr = simple_strtoul(argv[3], NULL, 16);
	if (dev < 0 || dev >= CONFIG_SYS_MAX_NAND_DEVICE || !nand_info[dev].name) {
		printf("\n** Device %d not available\n", dev);
		return 1;
	}

	nand = &nand_info[dev];
	printf("\nLoading from device %d: %s (offset 0x%lx)\n",
	       dev, nand->name, offset);

	block_num = offset / CONFIG_SYS_FLASH_SECTOR_SIZE;
	if (dni_nand_read_eb_manage_bad(
			nand, &block_num, (u_char *)addr)) {
		printf("** Read error on %d\n", dev);
		return 1;
	}
#ifdef CONFIG_HW29765235P0P512P1024P4X4P4X4	
	addr=addr+0x4;
#endif
	hdr = (image_header_t *)addr;
	kernel_partition_size = (((2 * sizeof(image_header_t) + ntohl(hdr->ih_size))
	           / CONFIG_SYS_FLASH_SECTOR_SIZE) + 1) * CONFIG_SYS_FLASH_SECTOR_SIZE;
	
	printf("\n** KERNEL partition size, kernel : 0x%x **\n", kernel_partition_size);
	
	if (!board_model_id_match_open_source_id() &&
	    kernel_partition_size > CONFIG_SYS_IMAGE_LEN) {
		printf("\n** Bad partition size, kernel : 0x%x **\n", kernel_partition_size);
		return 1;
	}

	addr_end = addr + kernel_partition_size;

	/* The first block is read. Start reading from the second block. */
	block_num++;
#ifdef CONFIG_HW29765235P0P512P1024P4X4P4X4
	addr = addr - 0x4;
#endif
	addr += CONFIG_SYS_FLASH_SECTOR_SIZE;

	while (addr < addr_end) {
		if (dni_nand_read_eb_manage_bad(
				nand, &block_num, (u_char *)addr)) {
			printf("** Read kernel partition error on %d\n", dev);
			return 1;
		}
		block_num++;
		addr += CONFIG_SYS_FLASH_SECTOR_SIZE;
	}

#ifdef CHECK_DNI_FIRMWARE_ROOTFS_INTEGRITY
	ulong rsize;

	if (board_model_id_match_open_source_id())
		return 0;

#ifdef CONFIG_HW29765235P0P512P1024P4X4P4X4
	addr_end = addr_end - 0x4;
#endif	
	hdr = (image_header_t *)(addr_end - sizeof(image_header_t));
	rsize = ntohl(hdr->ih_size);
#ifdef CONFIG_HW29765235P0P512P1024P4X4P4X4
	if( gpio_get_value(POWER_LED) ) {
		gpio_set_value(POWER_LED,0);
	} else {
		gpio_set_value(POWER_LED,1);
	}
#endif
	printf("\n** ROOTFS partition size, kernel: 0x%x, rootfs: 0x%x **\n", kernel_partition_size, rsize);
	if (rsize > (CONFIG_SYS_IMAGE_LEN - kernel_partition_size)) {
		printf("\n** Bad partition size, kernel: 0x%x, rootfs: 0x%x **\n", kernel_partition_size, rsize);
		return 1;
	}

	addr_end += rsize;
	while (addr < addr_end) {
		if (dni_nand_read_eb_manage_bad(
				nand, &block_num, (u_char *)addr)) {
			printf("** Read rootfs partition error on %d\n", dev);
			return 1;
		}
		block_num++;
		addr += CONFIG_SYS_FLASH_SECTOR_SIZE;
	}
#endif

	return 0;
}

U_BOOT_CMD(
	loadn_dniimg,	4,	0,	do_loadn_dniimg,
	"load dni firmware image from NAND.",
	"<device> <offset> <loadaddr>\n"
	"    - load dni firmware image that stored in NAND.\n"
	"    <device> : which NAND device.\n"
	"    <offset> : offset of the image in NAND.\n"
	"    <loadaddr> : address the image will be loaded to.\n"
);

int do_calculate_rootfs_uimage_address(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	ulong offset, addr, mem_addr, rootfs_addr;
	ulong block_num;
	image_header_t *hdr;
	nand_info_t *nand;
	int dev = 0;
	nand = &nand_info[dev];	

	offset = simple_strtoul(argv[1], NULL, 16);
	addr = simple_strtoul(argv[2], NULL, 16);
	mem_addr = simple_strtoul(argv[2], NULL, 16);

	block_num = offset / CONFIG_SYS_FLASH_SECTOR_SIZE;
	dni_nand_read_eb_manage_bad(nand, &block_num, (u_char *)addr);

#ifdef CONFIG_HW29765235P0P512P1024P4X4P4X4
	addr=addr+0x4;
#endif

	hdr = (image_header_t *)addr;
	
	rootfs_addr = (ntohl(hdr->ih_size)/CONFIG_SYS_FLASH_SECTOR_SIZE+1)*CONFIG_SYS_FLASH_SECTOR_SIZE+2*sizeof(image_header_t)-sizeof(image_header_t);
	rootfs_addr = rootfs_addr - (0x80 - mem_addr);
	
	printf("\n** rootfs address : 0x%x **\n", rootfs_addr);
	char runcmd[256];
	snprintf(runcmd, sizeof(runcmd), "setenv rootfs_addr_for_fw_checking 0x%x",rootfs_addr);
	run_command(runcmd,0);

	return rootfs_addr;
}

U_BOOT_CMD(
	calc_rootaddr,   3,  0, do_calculate_rootfs_uimage_address,
	"Calculate the DRAM address of rootfs uImage.",
	" <offset>\n"
	"    - Calculate the DRAM address of rootfs uImage w.r.t. <loadaddr>.\n"
	"    <offset> : offset of the image in NAND.\n"
	"    <loadaddr> : address the image will be loaded to.\n"
);

int chk_img (ulong addr)
{
	ulong data, len, checksum;
	image_header_t header;
	image_header_t *hdr = &header;

	memmove (&header, (char *)addr, sizeof(image_header_t));
	if (ntohl(hdr->ih_magic) != IH_MAGIC) {
		printf("\n** Bad Magic Number 0x%x **\n", hdr->ih_magic);
		return 1;
	}

	data = (ulong)&header;
	len  = sizeof(image_header_t);
	checksum = ntohl(hdr->ih_hcrc);
	hdr->ih_hcrc = 0;
	if (crc32 (0, (uchar *)data, len) != checksum) {
		puts ("\n** Bad Header Checksum **\n");
		return 1;
	}

	data = addr + sizeof(image_header_t);
	len  = ntohl(hdr->ih_size);
	puts ("   Verifying Checksum ... ");
	if (crc32 (0, (uchar *)data, len) != ntohl(hdr->ih_dcrc)) {
		puts ("   Bad Data CRC\n");
		return 1;
	}
	puts ("OK\n");

	return 0;
}

int do_chk_dniimg(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	ulong addr;

	if (board_model_id_match_open_source_id())
		return 0;

	if (argc != 2) {
		printf("Usage:\n%s\n", cmdtp->usage);
		return 1;
	}
	addr = simple_strtoul(argv[1], NULL, 16);

	printf("\n** check kernel image **\n");
	if (chk_img(addr)) {
		return 1;
	}

#ifdef CHECK_DNI_FIRMWARE_ROOTFS_INTEGRITY
	image_header_t *hdr;
	ulong kernel_partition_size;

	hdr = (image_header_t *)addr;
	kernel_partition_size = (((2 * sizeof(image_header_t) + ntohl(hdr->ih_size))
	           / CONFIG_SYS_FLASH_SECTOR_SIZE) + 1) * CONFIG_SYS_FLASH_SECTOR_SIZE;

	printf("\n** check rootfs image **\n");
	if (chk_img(addr + kernel_partition_size - sizeof(image_header_t))) {
		return 1;
	}
#endif

	return 0;
}

U_BOOT_CMD(
	chk_dniimg,	2,	0,	do_chk_dniimg,
	"check integrity of dni firmware image.",
	"<addr> - check integrity of dni firmware image.\n"
	"    <addr> : starting address of image.\n"
);

#ifdef CONFIG_DUAL_FIRMWARE
int do_bootdni(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	ulong flash_addr = CONFIG_SYS_IMAGE_BASE_ADDR;
	ulong second_flash_addr = CONFIG_SYS_IMAGE_2_BASE_ADDR;
	ulong load_addr = CONFIG_SYS_SDRAM_BASE;
#if defined(CONFIG_HW29766097P0P512P1024P2X2P2X2P2X2)
	load_addr = 0x42000000;
#endif
	ulong kernel_size = KERNEL_SIZE;
	int bootpart = 0;
	char runcmd[256];
	
	printf("Read BootPart from BOARD DATA...\n");
	bootpart = boot_partition();

	printf("BootPart = %d\n",bootpart);

	if (bootpart == 1)
	{
		printf("Loading DNI firmware for checking...\n");
		snprintf(runcmd, sizeof(runcmd), "loadn_dniimg 0 0x%lx 0x%lx", flash_addr, load_addr);
		run_command(runcmd, 0);
		snprintf(runcmd, sizeof(runcmd), "calc_rootadd 0x%lx 0x%lx", flash_addr, load_addr);
		run_command(runcmd, 0);
		snprintf(runcmd, sizeof(runcmd), "iminfo 0x%lx", load_addr);
		run_command(runcmd, 0);
		snprintf(runcmd, sizeof(runcmd), "if test $? -ne 0; then echo \"linux checksum error\"; boot_partition_set 2; fi;");
		run_command(runcmd, 0);
		snprintf(runcmd, sizeof(runcmd), "iminfo $rootfs_addr_for_fw_checking");
		run_command(runcmd, 0);
		snprintf(runcmd, sizeof(runcmd), "if test $? -ne 0; then echo \"rootfs checksum error\"; boot_partition_set 2; fi;");
		run_command(runcmd, 0);
		
		bootpart = boot_partition();
		if (bootpart == 1){
			snprintf(runcmd, sizeof(runcmd), "nand read 0x%lx 0x%lx 0x%lx", load_addr, flash_addr, kernel_size);
			run_command(runcmd, 0);
		}
		if (bootpart == 2){
			printf("Loading DNI firmware for checking...\n");
			snprintf(runcmd, sizeof(runcmd), "loadn_dniimg 0 0x%lx 0x%lx", second_flash_addr, load_addr);
			run_command(runcmd, 0);
			snprintf(runcmd, sizeof(runcmd), "calc_rootadd 0x%lx 0x%lx", second_flash_addr, load_addr);
			run_command(runcmd, 0);
			snprintf(runcmd, sizeof(runcmd), "iminfo 0x%lx", load_addr);
			run_command(runcmd, 0);
			snprintf(runcmd, sizeof(runcmd), "if test $? -ne 0; then echo \"linux checksum error\"; boot_partition_set 1; fw_recovery; fi;");
			run_command(runcmd, 0);
			snprintf(runcmd, sizeof(runcmd), "iminfo $rootfs_addr_for_fw_checking");
			run_command(runcmd, 0);
			snprintf(runcmd, sizeof(runcmd), "if test $? -ne 0; then echo \"rootfs checksum error\"; boot_partition_set 1; fw_recovery; fi;");
			run_command(runcmd, 0);
			bootpart = boot_partition();
			if (bootpart == 2){
				snprintf(runcmd, sizeof(runcmd), "nand read 0x%lx 0x%lx 0x%lx", load_addr, second_flash_addr, kernel_size);
				run_command(runcmd, 0);
			}
		}
	}

	if (bootpart == 2)
	{
		printf("Loading DNI firmware for checking...\n");
		snprintf(runcmd, sizeof(runcmd), "loadn_dniimg 0 0x%lx 0x%lx", second_flash_addr, load_addr);
		run_command(runcmd, 0);
		snprintf(runcmd, sizeof(runcmd), "calc_rootadd 0x%lx 0x%lx", second_flash_addr, load_addr);
		run_command(runcmd, 0);
		snprintf(runcmd, sizeof(runcmd), "iminfo 0x%lx", load_addr);
		run_command(runcmd, 0);
		snprintf(runcmd, sizeof(runcmd), "if test $? -ne 0; then echo \"linux checksum error\"; boot_partition_set 1; fi;");
		run_command(runcmd, 0);
		snprintf(runcmd, sizeof(runcmd), "iminfo $rootfs_addr_for_fw_checking");
		run_command(runcmd, 0);
		snprintf(runcmd, sizeof(runcmd), "if test $? -ne 0; then echo \"rootfs checksum error\"; boot_partition_set 1; fi;");
		run_command(runcmd, 0);
		
		bootpart = boot_partition();
		if (bootpart == 2){
			snprintf(runcmd, sizeof(runcmd), "nand read 0x%lx 0x%lx 0x%lx", load_addr, second_flash_addr, kernel_size);
			run_command(runcmd, 0);
		}
		if (bootpart == 1){
			printf("Loading DNI firmware for checking...\n");
			snprintf(runcmd, sizeof(runcmd), "loadn_dniimg 0 0x%lx 0x%lx", flash_addr, load_addr);
			run_command(runcmd, 0);
			snprintf(runcmd, sizeof(runcmd), "calc_rootadd 0x%lx 0x%lx", flash_addr, load_addr);
			run_command(runcmd, 0);
			snprintf(runcmd, sizeof(runcmd), "iminfo 0x%lx", load_addr);
			run_command(runcmd, 0);
			snprintf(runcmd, sizeof(runcmd), "if test $? -ne 0; then echo \"linux checksum error\"; boot_partition_set 2; fw_recovery; fi;");
			run_command(runcmd, 0);
			snprintf(runcmd, sizeof(runcmd), "iminfo $rootfs_addr_for_fw_checking");
			run_command(runcmd, 0);
			snprintf(runcmd, sizeof(runcmd), "if test $? -ne 0; then echo \"rootfs checksum error\"; boot_partition_set 2; fw_recovery; fi;");
			run_command(runcmd, 0);
			bootpart = boot_partition();
			if (bootpart == 1){
				snprintf(runcmd, sizeof(runcmd), "nand read 0x%lx 0x%lx 0x%lx", load_addr, flash_addr, kernel_size);
				run_command(runcmd, 0);
			}
		}
	}

	return 0;
}
U_BOOT_CMD(
	bootdni,   1,  0, do_bootdni,
	"Check DNI image file.",
	" <offset>\n"
	"    - Check the kernel and rootfs in image file.\n"
);
#endif

#endif	/* CHECK_DNI_FIRMWARE_INTEGRITY */

#if defined(HW_REVISION_OFFSET)
int do_hw_revision_set(cmd_tbl_t *cmdtp, int flag, int argc,
                            char * const argv[])
{
	u8 board_data[HW_REVISION_LENGTH + 1];
	int board_data_len = 0;

	if (argc != 2) {
		printf("%s\n",cmdtp->usage);
		return 1;
	}
	if ((board_data_len = strlen(argv[1])) > HW_REVISION_LENGTH) {
		printf ("the length of HW_REVISION can't > %d\n", HW_REVISION_LENGTH);
		return 1;
	}

	memset(board_data, 0, sizeof(board_data));
	memcpy(board_data, argv[1], board_data_len);

	printf("Burn HW_REVISION (= %s) into boarddata1 block\n", board_data);
	set_board_data(HW_REVISION_OFFSET, HW_REVISION_LENGTH, board_data);
	puts("Done.\n");
	return 0;
}

U_BOOT_CMD(
	hw_revision_set, 2, 0, do_hw_revision_set,
	"Set hw_revision on board",
	"XXXXXX\n"
	" For instance: hw_revision_set 01"
);

int do_hw_revision_show(cmd_tbl_t *cmdtp, int flag, int argc,
                             char * const argv[])
{
	u8 board_data[HW_REVISION_LENGTH + 1];
	int i = 0;

	memset(board_data, 0, sizeof(board_data));
	get_board_data(HW_REVISION_OFFSET, HW_REVISION_LENGTH, board_data);
	printf("hw_revision: ");
	for (i=0;i<HW_REVISION_LENGTH;i++)
		printf("%c", board_data[i]);
	printf("\n");
	return 0;
}

U_BOOT_CMD(
	hw_revision_show, 1, 0, do_hw_revision_show,
	"Show hw_revision",
	"\n"
	" For instance: hw_revision_show"
);
#endif
