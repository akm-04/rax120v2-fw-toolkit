#include <common.h>
/* TODO: can we just include all these headers whether needed or not? */
#if defined(CONFIG_CMD_BEDBUG)
#include <bedbug/type.h>
#endif
#include <command.h>
#include <console.h>
#ifdef CONFIG_HAS_DATAFLASH
#include <dataflash.h>
#endif
#include <dm.h>
#include <environment.h>
#include <fdtdec.h>
#if defined(CONFIG_CMD_IDE)
#include <ide.h>
#endif
#include <initcall.h>
#ifdef CONFIG_PS2KBD
#include <keyboard.h>
#endif
#if defined(CONFIG_CMD_KGDB)
#include <kgdb.h>
#endif
#include <logbuff.h>
#include <malloc.h>
#include <mapmem.h>
#ifdef CONFIG_BITBANGMII
#include <miiphy.h>
#endif
#include <mmc.h>
#include <nand.h>
#include <onenand_uboot.h>
#include <scsi.h>
#include <serial.h>
#include <spi.h>
#include <stdio_dev.h>
#include <trace.h>
#include <watchdog.h>
#ifdef CONFIG_CMD_AMBAPP
#include <ambapp.h>
#endif
#ifdef CONFIG_ADDR_MAP
#include <asm/mmu.h>
#endif
#include <asm/sections.h>
#ifdef CONFIG_X86
#include <asm/init_helpers.h>
#endif
#include <dm/root.h>
#include <linux/compiler.h>
#include <linux/err.h>
#ifdef CONFIG_AVR32
#include <asm/arch/mmu.h>
#endif

#ifdef FIRMWARE_RECOVER_FROM_TFTP_SERVER
static int factory_default = 0;
extern ulong time_start;
extern ulong time_delta;

extern thand_f *time_handler;

void start_tftp_recovery_mode()
{
    board_power_led(0);
    printf("\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b");
    StartTftpServerToRecoveFirmware ();/*Enter recovery mode when press reset button to upgrade mode*/
}

void board_reset_default_LedSet_slow(void);
#endif

int netgear_push_button(void)
{
#ifdef FIRMWARE_RECOVER_FROM_TFTP_SERVER
	int i,j;

	net_set_timeout_handler (CONFIG_SYS_HZ/10,board_reset_default_LedSet);
	for (j = 0;j < 12; j++) {
		if(j >= 5) {
			if( (j % 2) ==1)
				printf("Factory Reset Mode\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b");
			else
				printf("                  \b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b");

			factory_default = 1;
		}
		/*each cycle delay 1 s (100 * 10000 us)*/
		for (i = 0;i< 100;i++) {
			//reset button isn't pressed
			if(!board_reset_button_is_press())
			{
				goto next;
			}
			/*first cycle power on test led*/
			if(!i)
			{
				board_test_led(0);
			}

			if(j < 5) {
				if (time_handler && ((get_timer(0) - time_start) > time_delta)) {
					thand_f *x;
					x = board_reset_default_LedSet_slow;
					time_handler = (thand_f *)0;
					(*x)();
				}
			}
			if(j >= 5) {
				if (time_handler && ((get_timer(0) - time_start) > time_delta)) {
					thand_f *x;
					x = board_reset_default_LedSet;
					time_handler = (thand_f *)0;
					(*x)();
				}
			}
			udelay(10000);
		}
	}
	factory_default = 0;
	start_tftp_recovery_mode();
next:
	if(factory_default)
		board_reset_default();

#if defined(CONFIG_SYS_NMRP) && !defined(CONFIG_CMD_NMRP)
	StartNmrpClient();
#endif
#endif
	return 0;
}
