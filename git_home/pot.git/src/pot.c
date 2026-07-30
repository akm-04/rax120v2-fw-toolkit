/************************************************
 * functions implemented in POT flash partition *
 *        Delta Networks Ltd. (R) 2012          *
 ************************************************/

/***
 * NETGEAR SPEC Rev 10 page 189
 * ------------------------------------------------------------------------------------
 * In order to minimize the risk of corrupting the flash, a simple bit write operation (not erase / write)
 * should be used to change the POT value counter. For example, if the original state of the flash is all
 * zeroes, whenever we increment the POT counter / value, we simply set the lowest bit to 1. The next
 * increment will set the next bit to 1. Therefore, we will need 4320 bits or 540 bytes to store up to three
 * days of POT value. Since we are not using the complete erase and write operation, there is less chance
 * of flash corruption.
 * ------------------------------------------------------------------------------------
 *
 * The DUT's original state of MTD flash is all ones: 0xFFFFFFFF, 0xFFFFFFFF ....
 ***/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/un.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include "common.h"
#include "libmtd.h"

char pot_mtd_dev[128];

int find_good_nand_block(int devfd, int virtual_offset)
{
	int good_block, expected_block = virtual_offset / NAND_FLASH_BLOCKSIZE + 1;
       	int real_offset = -1, count;
	struct mtd_dev_info mtd_info;
	libmtd_t mtd_desc;

	mtd_desc = libmtd_open();
	if(mtd_get_dev_info(mtd_desc, pot_mtd_dev, &mtd_info) < 0){
		fprintf(stderr, "mtd_get_dev_info failed\n");
		return -1;
	}
	for (good_block = count = 0; good_block < expected_block && count < TOTAL_BLOCK_COUNT;
			count++){
		if(!mtd_is_bad(&mtd_info, devfd, count))
			good_block++;
	}
	if(good_block >= expected_block)
		real_offset = (count - 1) * NAND_FLASH_BLOCKSIZE + virtual_offset % NAND_FLASH_BLOCKSIZE; 

	return real_offset;
}


#if SUPPORT_EMMC_TYPE
void dni_erase_for_mmc(int devfd)
{
	int block = 0;
	unsigned char buf[512]={0};

	memset(buf,0xFF,512);

	while(block <= 512){
		write(devfd, buf, 512);
		block++;
	}
}

static void erase_pot_mtd(char *mtd)
{
	int devfd;
	struct mtd_info_user mtdInfo;
	struct erase_info_user mtdEraseInfo;

	devfd = open(mtd, O_RDWR |O_SYNC);
	if (devfd < 0)
		return;
#if 0
	if (ioctl(devfd, MEMGETINFO, &mtdInfo) == 0) {
		mtdEraseInfo.start = 0;
		mtdEraseInfo.length = mtdInfo.erasesize;	
		ioctl(devfd, MEMERASE, &mtdEraseInfo);
	}
#endif

	dni_erase_for_mmc(devfd);

	close(devfd);
}
#endif

int dni_erase_mtd_partition(int devfd)
{
	struct mtd_info_user mtdInfo;
	struct erase_info_user mtdEraseInfo;
	int skip_size, i;

	memset(&mtdInfo, 0x00, sizeof(struct mtd_info_user));
	memset(&mtdEraseInfo, 0x00, sizeof(struct erase_info_user));
	if (ioctl(devfd, MEMGETINFO, &mtdInfo) == 0){   // get POT MTD info; return 0 for NOR flash and -1 for NAND
		if ( MTD_NANDFLASH == mtdInfo.type ) { //NAND flash
			skip_size = find_good_nand_block(devfd, 0);   
			if(skip_size == -1){
				dfp("set_potval: no good block on %s from 0x%08x\n", 0);	
				return 0;
			}
			mtdEraseInfo.start = skip_size;
			mtdEraseInfo.length = mtdInfo.erasesize;
			if (ioctl(devfd, MEMERASE, &mtdEraseInfo)) {
				dfp("Failed to erase block on mtd from 0x%08x\n", mtdEraseInfo.start);
				return 0;
			}
			fprintf(stderr, "POT: dni_erase_mtd_partition erase block on mtd from 0x%08x size 0x%08x \n", mtdEraseInfo.start, mtdEraseInfo.length);

			skip_size = find_good_nand_block(devfd, NAND_FLASH_BLOCKSIZE);   
			if(skip_size == -1){
				dfp("set_potval: no good block on %s from 0x%08x\n", NAND_FLASH_BLOCKSIZE);	
				return 0;
			}
			mtdEraseInfo.start = skip_size;
			mtdEraseInfo.length = mtdInfo.erasesize;
			if (ioctl(devfd, MEMERASE, &mtdEraseInfo)) {
				dfp("Failed to erase block on mtd from 0x%08x\n", mtdEraseInfo.start);
				return 0;
			}
			fprintf(stderr, "POT: dni_erase_mtd_partition erase block2 on mtd from 0x%08x, size 0x%08x \n", mtdEraseInfo.start, mtdEraseInfo.length);

			skip_size = find_good_nand_block(devfd, FIRST_NTP_TIME_OFFSET);   
			if(skip_size == -1){
				dfp("set_potval: no good block on %s from 0x%08x\n", FIRST_NTP_TIME_OFFSET);	
				return 0;
			}
			mtdEraseInfo.start = skip_size;
			mtdEraseInfo.length = mtdInfo.erasesize;
			if (ioctl(devfd, MEMERASE, &mtdEraseInfo)) {
				dfp("Failed to erase block on mtd from 0x%08x\n", mtdEraseInfo.start);
				return 0;
			}
			fprintf(stderr, "POT: dni_erase_mtd_partition erase block3 on mtd from 0x%08x, size 0x%08x \n", mtdEraseInfo.start, mtdEraseInfo.length);
				
			/* Also erase remaining blocks for bad block handling. This is for the case
			 * that we can use good block direclty if new bad block is generated. Factory
			 * always set pot_value as 0, so all blocks are erased on shipped units.
			 */
			for(i = skip_size / NAND_FLASH_BLOCKSIZE + 1; i < TOTAL_BLOCK_COUNT; i++){
				mtdEraseInfo.start = i * NAND_FLASH_BLOCKSIZE;
				mtdEraseInfo.length = mtdInfo.erasesize;
				/* If fail to erase, don't care and contine erasing? */
				fprintf(stderr, "POT: dni_erase_mtd_partition erase block%d on mtd from 0x%08x, size 0x%08x \n", i+1, mtdEraseInfo.start, mtdEraseInfo.length);
				ioctl(devfd, MEMERASE, &mtdEraseInfo);
			}

			return 1;
		} else if ( MTD_NORFLASH == mtdInfo.type ) { //Nor flash
			mtdEraseInfo.start = 0;
			mtdEraseInfo.length = mtdInfo.erasesize;	
			ioctl(devfd, MEMERASE, &mtdEraseInfo);
			return 1;
		}
	}

	//emmc
	int block = 0;
	unsigned char buf[512];

	memset(buf,0xFF,512);
	while(block <= 512){
		write(devfd, buf, 512);
		block++;
	}
	return 1;
}

/* potd: functions */
static void update_to_file(int potval)
{
	FILE *fp;

	if (potval > POT_MAX_VALUE || potval < 0)
		potval = POT_MAX_VALUE;

	fp = fopen(POT_FILENAME, "w");
	if (fp == NULL) {
		fprintf(stderr, "Can't open " POT_FILENAME "!\n");
		return;
	}
	fprintf(fp, "%d", potval);

	fclose(fp);
}

static void set_page_data(int potval, unsigned char *buf)
{
	unsigned int word, *wordp;

	wordp = (unsigned int *)buf;
	word = (unsigned int)potval;
	wordp[0] = word;

}

void pot_usage(char *name)
{
	fprintf(stderr,
		"\nUsage: %s <-d mtd_block>\n",
		name);
	exit(0);
}

int pot_func(int argc, char **argv)
{
	int ch;
	int devfd;
	int pot_value;
	int word_len;
	int bitone_len;
	struct timeval timo;
	unsigned int word_value, *wordp;
	unsigned char buf[NAND_FLASH_PAGESIZE]={0};
	struct mtd_info_user mtdInfo;
	struct erase_info_user mtdEraseInfo;
	int offset=0, i, skip_size = 0;
	char mtd[32] = {0};

	fprintf(stderr, "POT is Running...\n");

#if SUPPORT_DNITOOLBOX
	devfd = open(pot_mtd_dev, O_RDWR | O_SYNC);
	if (devfd < 0) {
		dfp("Can't open POT mtd - %s\n",pot_mtd_dev);
		return -1;
	}
#else
	while ((ch = getopt(argc, argv, "d:")) != -1) {
		switch(ch) {
			case 'd':
				strncpy(mtd, optarg, sizeof(mtd)-1);
				break;
			default:
				break;
		}
	}
	/* Return if mtd is empty */
	if (strlen(mtd) == 0)
		pot_usage(argv[0]);
	dfp("mtd:%s\n", mtd);
	devfd = open(mtd, O_RDWR | O_SYNC);
	if (devfd < 0) {
		dfp("Can't open POT mtd - %s\n", mtd);
		return -1;
	}
#endif

#if SUPPORT_EMMC_TYPE
	word_len = -1;
	do{
		word_len++;
		if(word_len >= NTPTIME_POSTION_BYINT){
			erase_pot_mtd(pot_mtd_dev);
			word_len = -1;
			continue;
		}
		read(devfd, &word_value, sizeof(word_value));
	} while (word_value == 0x00000000);

	/* point to first word which isn't 0x00000000 */
	lseek(devfd, -4, SEEK_CUR);

	if (word_value == 0xFFFFFFFF)
		bitone_len = 32;
	else {
		bitone_len = 0;
		do {
			bitone_len++;
			word_value = word_value >> 1;
		} while (word_value);
	}

	pot_value = (word_len * 32) + (32 - bitone_len);
	if (pot_value >= POT_MAX_VALUE)
		goto fin;

	update_to_file(pot_value);
	for (;;) {
		timo.tv_sec = POT_RESOLUTION  * 60;
		timo.tv_usec = 1;
		select(1, NULL, NULL, NULL, &timo);

		/* need verify the result of read & write operation ?_? */
		read(devfd, &word_value, sizeof(word_value));
		lseek(devfd, -4, SEEK_CUR);
		word_value = word_value >> 1;
		write(devfd, &word_value, sizeof(word_value));

		if (word_value != 0x00000000)
			lseek(devfd, -4, SEEK_CUR);

		pot_value++;
		if (pot_value >= POT_MAX_VALUE)
			goto fin;
		update_to_file(pot_value);
	}
#else 

	skip_size = find_good_nand_block(devfd, 0);
	if(skip_size == -1){
		fprintf(stderr, "NTPST: no good block, exit now\n");	
		exit(1);
	}

	/* locate to the last page which have the values */
	for (i=0; i < NAND_FLASH_BLOCKSIZE/NAND_FLASH_PAGESIZE; i++) {
		lseek(devfd, offset + skip_size, SEEK_SET);
		read(devfd, buf, sizeof(buf));
		wordp = (unsigned int *)buf;
		word_value = wordp[0];
		if (word_value == 0xFFFFFFFF)
			break;
		offset += NAND_FLASH_PAGESIZE;
	}

	if (offset == 0)
		lseek(devfd, NAND_FLASH_BLOCKSIZE + skip_size, SEEK_SET);
	else
		lseek(devfd, offset-NAND_FLASH_PAGESIZE + skip_size, SEEK_SET);

	/**
	 * count the number of word (4 bytes), not 0x00000000 at the head, and read the
	 * first word which isn't ZERO.
	 */
	read(devfd, buf, sizeof(buf));
	wordp = (unsigned int *)buf;
	word_value = wordp[0];
	if (word_value == 0xFFFFFFFF) {
		word_value = 0;
	}

	pot_value = word_value;
	if(pot_value == 0) {
		fprintf(stderr, "POT first time running, erase whole pot partition first!\n");
		dni_erase_mtd_partition(devfd);
	}

	update_to_file(pot_value < POT_MAX_VALUE ? pot_value : POT_MAX_VALUE);
	if (pot_value >= POT_MAX_VALUE)
		goto fin;

	memset(&mtdInfo, 0x00, sizeof(struct mtd_info_user));
	memset(&mtdEraseInfo, 0x00, sizeof(struct erase_info_user));
	if (ioctl(devfd, MEMGETINFO, &mtdInfo)) {
		dfp("can't get mtd device info from %s\n", mtd);
		goto fin;
	}

	for (;;) {
		timo.tv_sec = POT_RESOLUTION  * 60;
		timo.tv_usec = 1;
		select(1, NULL, NULL, NULL, &timo);

		/* need verify the result of read & write operation ?_? */
		if (offset == 0 || offset == NAND_FLASH_BLOCKSIZE) {
			offset = 0;
			/* store the next pot value to another block */
			mtdEraseInfo.start = NAND_FLASH_BLOCKSIZE + skip_size;
			mtdEraseInfo.length = mtdInfo.erasesize;
			fprintf(stderr, "POTD backup action, erase block2 from 0x%08x size 0x%08x, backup pot value %d!\n", mtdEraseInfo.start, mtdEraseInfo.length, pot_value+1);
			if (ioctl(devfd, MEMERASE, &mtdEraseInfo)) {
				dfp("Failed to erase block on %s from 0x%08x\n", mtd, mtdEraseInfo.start);
				goto fin;
			}
			set_page_data(pot_value+1, buf);
			lseek(devfd, NAND_FLASH_BLOCKSIZE + skip_size, SEEK_SET);
			write(devfd, buf, sizeof(buf));

			/*erase whole block1 to save pot value*/
			mtdEraseInfo.start = 0 + skip_size;
			mtdEraseInfo.length = mtdInfo.erasesize;
			fprintf(stderr, "POTD write to block's last page, erase block1 from 0x%08x size 0x%08x!\n", mtdEraseInfo.start, mtdEraseInfo.length);
			if (ioctl(devfd, MEMERASE, &mtdEraseInfo)) {
				dfp("Failed to erase block on %s from 0x%08x\n", mtd, mtdEraseInfo.start);
				goto fin;
			}
		}

		pot_value++;
		if (pot_value >= POT_MAX_VALUE)
			goto fin;

		update_to_file(pot_value);
		set_page_data(pot_value, buf);
		lseek(devfd, offset + skip_size, SEEK_SET);
		write(devfd, buf, sizeof(buf));
		offset += NAND_FLASH_PAGESIZE;
	}
#endif

fin:
	update_to_file(pot_value);
	fprintf(stderr, "POT is Finished!!!\n");

	close(devfd);
	return 0;
}


/* potval: functions */
static void set_potval(char *value, char *mtd)
{
	int devfd;
	int pot_value, data;
	int len, word;
	unsigned char buf[NAND_FLASH_PAGESIZE]={0};
	struct mtd_info_user mtdInfo;
	struct erase_info_user mtdEraseInfo;
	int page_num, offset = 0, i;
	int skip_size;

	pot_value = atoi(value);
	if (pot_value < 0 || pot_value > POT_MAX_VALUE)
		return;

	/* stop the POT Demo firstly. */
	system("/usr/bin/killall potd 2> /dev/null");

	/****************************************/
#if SUPPORT_EMMC_TYPE
	erase_pot_mtd(pot_mtd_dev);

	/****************************************/
	devfd = open(pot_mtd_dev,O_RDWR);
	if (devfd < 0)
		goto start;

	len = pot_value >> 5;
	word = 0x00000000;
	while (len--)
		write(devfd, &word, sizeof(word));
	len = pot_value & 31;
	word = 0xFFFFFFFF >> len;
	write(devfd, &word, sizeof(word));
#else 
	devfd = open(mtd, O_RDWR | O_SYNC);
	if (devfd < 0)
		goto start;

	memset(&mtdInfo, 0x00, sizeof(struct mtd_info_user));
	memset(&mtdEraseInfo, 0x00, sizeof(struct erase_info_user));

	if (ioctl(devfd, MEMGETINFO, &mtdInfo)) {
		dfp("can't get mtd device info from %s\n", mtd);
		goto start;
	}

	skip_size = find_good_nand_block(devfd, 0);   
	if(skip_size == -1){
		dfp("set_potval: no good block on %s from 0x%08x\n", 0);	
		goto start;
	}
	offset = skip_size;
	mtdEraseInfo.start = skip_size;
	mtdEraseInfo.length = mtdInfo.erasesize;
	if (ioctl(devfd, MEMERASE, &mtdEraseInfo)) {
		dfp("Failed to erase block on %s from 0x%08x\n", mtd, mtdEraseInfo.start);
		goto start;
	}

	skip_size = find_good_nand_block(devfd, NAND_FLASH_BLOCKSIZE);   
	if(skip_size == -1){
		dfp("set_potval: no good block on %s from 0x%08x\n", NAND_FLASH_BLOCKSIZE);	
		goto start;
	}

	mtdEraseInfo.start = skip_size;
	mtdEraseInfo.length = mtdInfo.erasesize;
	if (ioctl(devfd, MEMERASE, &mtdEraseInfo)) {
		dfp("Failed to erase block on %s from 0x%08x\n", mtd, mtdEraseInfo.start);
		goto start;
	}


	skip_size = find_good_nand_block(devfd, FIRST_NTP_TIME_OFFSET);   
	if(skip_size == -1){
		dfp("set_potval: no good block on %s from 0x%08x\n", FIRST_NTP_TIME_OFFSET);	
		goto start;
	}

	mtdEraseInfo.start = skip_size;
	mtdEraseInfo.length = mtdInfo.erasesize;
	if (ioctl(devfd, MEMERASE, &mtdEraseInfo)) {
		dfp("Failed to erase block on %s from 0x%08x\n", mtd, mtdEraseInfo.start);
		goto start;
	}

	if (pot_value == 0){
		for(i = skip_size / NAND_FLASH_BLOCKSIZE + 1; i < TOTAL_BLOCK_COUNT; i++){
			mtdEraseInfo.start = i * NAND_FLASH_BLOCKSIZE;
			mtdEraseInfo.length = mtdInfo.erasesize;
			/* If fail to erase, don't care and contine erasing? */
			ioctl(devfd, MEMERASE, &mtdEraseInfo);
		}

		goto start;
	}

	page_num = pot_value % (NAND_FLASH_BLOCKSIZE/NAND_FLASH_PAGESIZE);

	if (page_num == 0)
		page_num = (NAND_FLASH_BLOCKSIZE/NAND_FLASH_PAGESIZE);

	for (i=page_num; i>0; i--) {
		data = pot_value - i + 1;
		set_page_data(data, buf);
		lseek(devfd, offset, SEEK_SET);
		write(devfd, buf, sizeof(buf));
		offset += NAND_FLASH_PAGESIZE;
	}
#endif

start:
	close(devfd);
	system("/usr/sbin/potd");
}

time_t get_ntpsynctime(char *mtd)
{
	time_t ntp = 0;
	int fd = 0;
	int skip_size;

	fd = open(mtd, O_RDWR | O_SYNC);
	if (0 > fd) {
		dfp("can't open mtd device - %s\n", mtd);
		ntp = 0xffffffff;
	} else {
#if SUPPORT_EMMC_TYPE
		lseek(fd, NTPTIME_POSTION, SEEK_SET);
#else
		skip_size = find_good_nand_block(fd, FIRST_NTP_TIME_OFFSET);   
		if(skip_size == -1){
			dfp("get_ntpsynctime: no good block\n");	
			ntp = 0xffffffff;
		}
		lseek(fd, skip_size, SEEK_SET);
#endif
		read(fd, &ntp, sizeof(ntp));
		close(fd);
	}
	return ntp;
}

void get_stamac(unsigned char* mac, char *mtd)
{
	int fd = 0;
	int skip_size;

	if (!mac)
		return;

	fd = open(mtd, O_RDWR | O_SYNC);
	if (0 > fd) {
		dfp("can't open mtd device - %s\n", mtd);
		memset(mac, 0xff, 6);
	} else {
#if SUPPORT_EMMC_TYPE
		lseek(fd, STAMAC_POSTION, SEEK_SET);
#else
		skip_size = find_good_nand_block(fd, FIRST_WIFISTATION_MAC_OFFSET);   
		if(skip_size == -1){
			dfp("get_stamac: no good block\n");	
			memset(mac, 0xff, 6);
		}

		lseek(fd, skip_size, SEEK_SET);
#endif
		read(fd, mac, 6);
		close(fd);
	}
}

void potval_usage(char *name)
{
	fprintf(stderr,
		"\nUsage: %s <-T timezone> <-d mtd_block>\n",
		name);
	exit(0);
}

int potval_func(int argc, char **argv)
{
	int ch;
	FILE *fp;
	fd_set readable;
	int r, listen_fd, conn_fd;
	char recvbuf[128]={0}, potval[128]={0};
	struct sockaddr_in addr;
	time_t ntptime;
	char strtime[64] = {0};
	unsigned char mac[6]={0};
	const unsigned char nomac[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
	char timezone[64] = {0}, mtd[32] = {0};
	char tz_env[64];

#if SUPPORT_DNITOOLBOX
	while ((ch = getopt(argc, argv, "T:")) != -1) {
		switch(ch) {
			case 'T':
				memset(timezone, 0, sizeof(timezone));
				strncpy(timezone, optarg, sizeof(timezone)-1);
				break;
			default:
				break;
		}
	}
	if (timezone[0] == '\0') {
		strncpy(timezone, "GMT+0", sizeof(timezone)-1);
		dfp("don't provide time zone,use default %s\n", timezone);
	}
#else
	while ((ch = getopt(argc, argv, "T:d:")) != -1) {
		switch(ch) {
			case 'T':
				memset(timezone, 0, sizeof(timezone));
				strncpy(timezone, optarg, sizeof(timezone)-1);
				break;
			case 'd':
				strncpy(mtd, optarg, sizeof(mtd)-1);
				break;
			default:
				break;
		}
	}
	if ((timezone[0] == '\0') || (mtd[0] == '\0')) {
		potval_usage(argv[0]);	
	}
#endif
	listen_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_fd < 0) {
		printf("error socket");
		return -1;
	}

	fcntl(listen_fd, F_SETFD, 1);
	memset(&addr, 0, sizeof(struct sockaddr_in));
	addr.sin_family = AF_INET;
	addr.sin_port   = htons(POT_PORT);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0 ||
	    listen(listen_fd, 10) < 0) {
		printf("Can't bind the POT socket");
		close(listen_fd);
		return -1;
	}

	fprintf(stderr, "potval: The POT-(Get/Set) Demo is Running ...\n");

	for (;;) {
		FD_ZERO(&readable);
		FD_SET(listen_fd, &readable);

		if (select(listen_fd + 1, &readable, NULL, NULL, NULL) < 1 ||
		    (conn_fd = accept(listen_fd, NULL, NULL)) < 0)
			continue;

		memset(potval, 0x00, sizeof(potval));
		memset(strtime, 0x00, sizeof(strtime));
		if ((fp = fopen(POT_FILENAME, "r")) == NULL)
			goto cont;
		if (!fgets(potval, sizeof(potval), fp))
			potval[0] = '\0';
		fclose(fp);
		strncat(potval, "NTP", sizeof("NTP"));
#if SUPPORT_DNITOOLBOX
		ntptime = get_ntpsynctime(pot_mtd_dev);
#else
		ntptime = get_ntpsynctime(mtd);
#endif
		if (0xffffffffffffffff == ntptime) {
			strncpy(strtime, "00-00-00", sizeof(strtime)-1);
		} else {
			snprintf(tz_env, sizeof(tz_env), "TZ=%s", timezone);
			putenv(tz_env);
			printf("Current NTP time_zone = %s \n",timezone);
			strftime(strtime, sizeof(strtime), "%T, %b %d, %Y", localtime(&ntptime));

		}
		strncat(potval, strtime, strnlen(strtime, sizeof(strtime)));

#if SUPPORT_DNITOOLBOX
		get_stamac(mac, pot_mtd_dev);
#else
		get_stamac(mac, mtd);
#endif
		if(!memcmp(nomac, mac, 6))
			memset(mac, 0, 6);
		snprintf(potval + strnlen(potval, sizeof(potval)), sizeof(potval) - strnlen(potval, sizeof(potval)), "MAC%02x-%02x-%02x-%02x-%02x-%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

		send(conn_fd, potval, strlen(potval), 0);
		r = recv(conn_fd, recvbuf, sizeof(recvbuf) - 1, 0);
		if (r < 1)
			goto cont;
		recvbuf[r] = '\0';
		/* printf("POT Recv'd Data : %s\n", recvbuf); */
		if (strcmp(recvbuf, "get"))
#if SUPPORT_DNITOOLBOX
			set_potval(recvbuf, pot_mtd_dev);
#else
			set_potval(recvbuf, mtd);
#endif
	cont:
		close(conn_fd);
	}

	shutdown(listen_fd, 2);
	close(listen_fd);
	return 0;
}

void pot_tool_usage(char *name)
{
	fprintf(stderr,
		"\nUsage: %s get / reset \n",
		name);
	exit(0);
}

int pot_tool_get()
{
	char potval[128] = {0}, strtime[64] = {0}, tz_env[64] = {0}, timezone[64] = {0};
	unsigned char mac[6]={0};
	time_t ntptime;
	const unsigned char nomac[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
	FILE *fp;

	memset(potval, 0x00, sizeof(potval));
	memset(strtime, 0x00, sizeof(strtime));
	if ((fp = fopen(POT_FILENAME, "r")) == NULL)
		return -1;
	if (!fgets(potval, sizeof(potval), fp))
		potval[0] = '\0';
	fclose(fp);

	fprintf(stdout, "POT: %s\n", potval);
	
	strncpy(timezone, config_get("time_zone"), sizeof(timezone)-1);
	ntptime = get_ntpsynctime(pot_mtd_dev);
	if (0xffffffffffffffff == ntptime) {
		strncpy(strtime, "00-00-00", sizeof(strtime)-1);
	} else {
		snprintf(tz_env, sizeof(tz_env), "TZ=%s", timezone);
		putenv(tz_env);
		//printf("Current NTP time_zone = %s \n",timezone);
		strftime(strtime, sizeof(strtime), "%T, %b %d, %Y", localtime(&ntptime));

	}
	
	fprintf(stdout, "NTP synchronized date/time: %s\n", strtime);

	get_stamac(mac, pot_mtd_dev);
	if(!memcmp(nomac, mac, 6))
		memset(mac, 0, 6);
	fprintf(stdout, "First associated STA: %02x-%02x-%02x-%02x-%02x-%02x\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	
	return 0;
}

int pot_tool_reset()
{
	char recvbuf[8] = {0};
	
	set_potval(recvbuf, pot_mtd_dev);
	sleep(2);
	pot_tool_get();
}

int pot_tool_func(int argc, char **argv)
{
	char cmd[32] = {0};
	
	if(argv[1] == NULL) {
		pot_tool_usage(argv[0]);
		return 0;
	} else
		strncpy(cmd, argv[1], sizeof(cmd)-1);

	if (strncmp(cmd, "get", sizeof("get")) == 0)
		pot_tool_get();
	else if (strncmp(cmd, "reset", sizeof("reset")) == 0)
		pot_tool_reset();
	else
		pot_tool_usage(argv[0]);	
	
	return 0;
}

int main(int argc, char *argv[])
{
#if SUPPORT_DNITOOLBOX
	FILE *fp = popen("part_dev pot", "r");
	if (fp) {
		fgets(pot_mtd_dev, sizeof(pot_mtd_dev), fp);
		pclose(fp);
	}
#if SUPPORT_NAND_TYPE || SUPPORT_NOR_TYPE
	if (strncmp("/dev/mtd", pot_mtd_dev, 8) != 0) {
		dfp("fail to get partition device!");
		exit(EXIT_FAILURE);
	}
#endif
#endif

	daemon(1, 1);

	if (strstr(argv[0], "ntpst") != NULL)
		return ntpst_func(argc, argv);
	else if (strstr(argv[0], "stamac") != NULL)
		return stamac_func(argc, argv);
	else if (strstr(argv[0], "potval") != NULL)
		return potval_func(argc, argv);
	else if (strstr(argv[0], "potd") != NULL)
		return pot_func(argc, argv);
	else if (strstr(argv[0], "netconn") != NULL){
	#if SUPPORT_EMMC_TYPE
		return netconn_func_for_emmc(argc, argv);
	#else
		return netconn_func_for_nand(argc, argv);
	#endif
	}
	else if (strstr(argv[0], "pot_tool") != NULL)
		return pot_tool_func(argc, argv);
	else
		dfp("command name is wrong!\n");

	return -1;
}
