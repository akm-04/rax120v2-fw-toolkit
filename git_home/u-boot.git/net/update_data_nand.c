#include <common.h>
#include "nmrp.h"
#include <net.h>
extern int NmrpState;
extern ulong NmrpAliveTimerStart;
extern ulong NmrpAliveTimerBase;

#include <nand.h>
#ifdef FIRMWARE_RECOVER_FROM_TFTP_SERVER
#include <dni_common.h>
extern int flash_sect_erase (ulong, ulong);

/**
 * handle_nand_modify_error:
 *
 * Handle erase or write error occured in a NAND erase block.
 *
 * For now, following method is adopted:
 *
 *     * Read the block again. If error, mark the block as bad and reset
 *       board.
 *
 *     * Optionally, if original data which is supposed to be written into the
 *       block is provided, compare read data with it. If 2 data are
 *       different, mark the block as bad and reset board.
 *
 *     * "mark the block as bad and reset board" above takes effect only when
 *       markbad function is implemented in NAND flash driver. If markbad is
 *       not implemented, nothing happens so that behaviors in old version of
 *       code are preserved.
 *
 * @param nand       NAND device
 * @param offset     offset in flash
 * @param orig_data  buffer containing data before being written.
 *                   pass NULL if you do not want to verify written data.
 * @return           never return if block is being tried to be marked as bad
 */
static void handle_nand_modify_error(nand_info_t *nand, ulong offset,
                                     uchar *orig_data)
{
	int rval;
	size_t read_length = CONFIG_SYS_FLASH_SECTOR_SIZE;
	uchar buffer[CONFIG_SYS_FLASH_SECTOR_SIZE];

	printf("Try to read block 0x%lx ... ", offset);
	rval = nand_read(nand, offset, &read_length, buffer);

	/* ECC-correctable block */
	if (rval == -EUCLEAN) {
		rval = 0;
	}
	printf("%s\n", rval ? "ERROR" : "OK");

	if (rval == 0 && orig_data != NULL) {
		puts("Compare written data with original data ... ");
		rval = memcmp(orig_data, buffer,
		              CONFIG_SYS_FLASH_SECTOR_SIZE);
		printf("%s\n", rval ? "DIFFERENT" : "SAME");
	}
}

void update_data(ulong addr, int data_size, ulong target_addr_begin, size_t target_addr_len, int send_nmrp_alive, int mark_bad_reset)
{
	int offset_num;
	uchar *src_addr;
	ulong target_addr;

	if (data_size <= 1) {
		printf("Incorrect data size\n");
		return;
	}

	target_addr = target_addr_begin;
	for (offset_num = 0;
	     offset_num < (((data_size - 1) / CONFIG_SYS_FLASH_SECTOR_SIZE) + 1);
	     offset_num++) {
		nand_erase_options_t nand_erase_options;
		size_t write_size;
		int ret = 0;

		/* erase 64K */
		while (nand_block_isbad(&nand_info[0], target_addr)) {
			printf("Skipping erasing bad block at 0x%08lx\n", target_addr);
			target_addr += CONFIG_SYS_FLASH_SECTOR_SIZE;
		}
		if (target_addr >= target_addr_begin + target_addr_len)
			goto bad_nand;

		printf("Erasing: off %x, size %x\n", target_addr, CONFIG_SYS_FLASH_SECTOR_SIZE);
		memset(&nand_erase_options, 0, sizeof(nand_erase_options));
		nand_erase_options.length = CONFIG_SYS_FLASH_SECTOR_SIZE;
		nand_erase_options.quiet = 0;
		nand_erase_options.jffs2 = 1;
		nand_erase_options.scrub = 0;
		nand_erase_options.offset = target_addr;
		ret = nand_erase_opts(&nand_info[0], &nand_erase_options);
		printf("%s\n", ret ? "ERROR" : "OK");

		if (mark_bad_reset && ret) {
			handle_nand_modify_error(
				&nand_info[0], target_addr, NULL);
		}

		src_addr = addr + offset_num * CONFIG_SYS_FLASH_SECTOR_SIZE;

		printf("Writing: from RAM addr %x, to NAND off %x, size %x\n", src_addr, target_addr, CONFIG_SYS_FLASH_SECTOR_SIZE);
		write_size = CONFIG_SYS_FLASH_SECTOR_SIZE;
		{
			char runcmd[256];
			int rval = 0;

			printf("Run nand write 0x%lx 0x%lx 0x%lx\n", src_addr, target_addr, write_size);
			rval= nand_write(&nand_info[0], target_addr, &write_size, (u_char *)src_addr);

			if (rval != 0) {
				printf("NAND write to offset %llx failed %d\n",	target_addr, rval);
				ret = 1;
			}
		}
		if (mark_bad_reset && ret) {
			handle_nand_modify_error(
				&nand_info[0], target_addr, src_addr);
		}

		CheckNmrpAliveTimerExpire(send_nmrp_alive);
		target_addr += CONFIG_SYS_FLASH_SECTOR_SIZE;
	}
	return;
bad_nand:
	printf("** FAIL !! too many bad blocks, no enough space for data.\n");
}

void update_firmware(ulong addr, int firmware_size)
{
	if (get_len_incl_bad(&nand_info[0], (loff_t)CONFIG_SYS_IMAGE_ADDR_BEGIN,
	    (size_t)firmware_size) > ((size_t)CONFIG_SYS_IMAGE_LEN +
	                              (size_t)board_image_reserved_length()))
	{
		printf("** FAIL !! too many bad blocks, no enough space for firmware image.\n");
		return;
	}

	update_data(addr, firmware_size, CONFIG_SYS_IMAGE_ADDR_BEGIN,
	            CONFIG_SYS_IMAGE_LEN +
		    (size_t)board_image_reserved_length(), 1, 1);

#ifdef CONFIG_DUAL_FIRMWARE
	printf ("boot_partition_set 1\n");
	run_command("boot_partition_set 1", 0);
#endif

#ifdef CONFIG_SYS_NMRP
	if(NmrpState != 0)
		return;
#endif
	printf ("Done\nRebooting...\n");

	do_reset(NULL,0,0,NULL);
}

#ifdef CONFIG_DUAL_FIRMWARE
void update_firmware_second(ulong addr, int firmware_size)
{
	if (get_len_incl_bad(&nand_info[0], (loff_t)CONFIG_SYS_IMAGE_2_ADDR_BEGIN,
	    (size_t)firmware_size) > ((size_t)CONFIG_SYS_IMAGE_LEN +
	                              (size_t)board_image_reserved_length()))
	{
		printf("** FAIL !! too many bad blocks, no enough space for firmware image.\n");
		return;
	}

	update_data(addr, firmware_size, CONFIG_SYS_IMAGE_2_ADDR_BEGIN,
	            CONFIG_SYS_IMAGE_LEN +
		    (size_t)board_image_reserved_length(), 1, 1);

	printf ("boot_partition_set 2\n");
	run_command("boot_partition_set 2", 0);

#ifdef CONFIG_SYS_NMRP
	if(NmrpState != 0)
		return;
#endif
	printf ("Done\nRebooting...\n");

	do_reset(NULL,0,0,NULL);
}
#endif  /*CONFIG_DUAL_FIRMWARE*/
#endif 
