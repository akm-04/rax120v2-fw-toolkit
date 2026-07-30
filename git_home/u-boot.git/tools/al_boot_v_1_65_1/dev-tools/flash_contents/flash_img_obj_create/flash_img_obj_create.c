#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "al_flash_contents.h"
#include "al_hal_reg_utils.h"

#define INVALID		0xffffffff

#define SZ_1K		1024

#define TEMP_BUFF_SIZE	SZ_1K

static uint8_t temp_buff[TEMP_BUFF_SIZE];

static uint32_t chksum32(uint8_t *buff, int len)
{
	uint32_t val = 0;

	for (; len; len--, buff++)
		val += *buff;

	return val;
}

static uint32_t le(uint32_t val)
{
	uint32_t le_val;
	int i;

	for (i = 0; i < 4; i++, val >>= 8)
		((uint8_t *)&le_val)[i] = val & 0xff;

	return le_val;
}

static int read_file_to_buff(char *file_name, uint8_t **buff, long *file_size)
{
	size_t num_bytes_read;
	int err = 0;
	FILE *f;

	f = fopen(file_name, "rb");
	if (!f)	{
		printf("Unable to open input file (%s)!\n", file_name);
		err = -EIO;
		goto done;
	}
	if (fseek(f, 0, SEEK_END)) {
		printf("Unable to obtain input file size (%s)!\n", file_name);
		err = -EIO;
		goto done_close_input_file;
	}
	if ((*file_size = ftell(f)) < 0) {
		printf("Unable to obtain input file size (%s)!\n", file_name);
		err = -EIO;
		goto done_close_input_file;
	}
	if (fseek(f, 0, SEEK_SET)) {
		printf("Unable to obtain input file size (%s)!\n", file_name);
		err = -EIO;
		goto done_close_input_file;
	}

	*buff = malloc(*file_size);
	if (!(*buff)) {
		printf("Unable to allocate buffer for file (%s)!\n", file_name);
		err = -ENOMEM;
		goto done_close_input_file;
	}

	num_bytes_read = fread(*buff, 1, (*file_size), f);
	if (num_bytes_read != (*file_size)) {
		printf("Read failed (%s)!\n", file_name);
		err = -EIO;
		goto done_free;
	}

	goto done_close_input_file;

done_free:
	free(*buff);

done_close_input_file:
	fclose(f);

done:
	return err;
}

static int write_buff(FILE *f, uint8_t *buff, long buff_size, long padded_size)
{
	size_t num_bytes_written;
	int err = 0;

	printf("%s(%p, %p, %08lx, %08lx)\n", __func__, f, buff, buff_size,  padded_size);

	if (buff_size > 0) {
		num_bytes_written = fwrite(buff, 1, buff_size, f);
		if (num_bytes_written != buff_size) {
			printf("Write failed!\n");
			err = -EIO;
			goto done;
		}
	}

	memset(temp_buff, 0xff, TEMP_BUFF_SIZE);

	while (padded_size > buff_size) {
		size_t num_bytes_to_write = padded_size - buff_size;

		if (num_bytes_to_write > TEMP_BUFF_SIZE)
			num_bytes_to_write = TEMP_BUFF_SIZE;

		num_bytes_written = fwrite(temp_buff, 1, num_bytes_to_write, f);
		if (num_bytes_written != num_bytes_to_write) {
			printf("Write failed!\n");
			err = -EIO;
			goto done;
		}

		padded_size -= num_bytes_to_write;
	}

done:
	return err;
}

static int write_buff_get_csum(FILE *f, uint8_t *buff, long buff_size, long padded_size, uint32_t *csum)
{
	size_t num_bytes_written;
	int err = 0;

	printf("%s(%p, %p, %08lx, %08lx, %p)\n", __func__, f, buff, buff_size,  padded_size, csum);

	*csum = 0;

	if (buff_size > 0) {
		num_bytes_written = fwrite(buff, 1, buff_size, f);
		if (num_bytes_written != buff_size) {
			printf("Write failed!\n");
			err = -EIO;
			goto done;
		}
		*csum += chksum32(buff, buff_size);
	}

	memset(temp_buff, 0xff, TEMP_BUFF_SIZE);

	while (padded_size > buff_size) {
		size_t num_bytes_to_write = padded_size - buff_size;

		if (num_bytes_to_write > TEMP_BUFF_SIZE)
			num_bytes_to_write = TEMP_BUFF_SIZE;

		num_bytes_written = fwrite(temp_buff, 1, num_bytes_to_write, f);
		if (num_bytes_written != num_bytes_to_write) {
			printf("Write failed!\n");
			err = -EIO;
			goto done;
		}
		*csum += chksum32(temp_buff, num_bytes_to_write);

		padded_size -= num_bytes_to_write;
	}

done:
	return err;
}

void syntax_err(char *mess)
{
	printf("Syntax error: %s\n\n", mess);

	printf("Syntax: flash_img_obj_create [options]\n\n");
	printf("--input_file <flash image object input file>\n");
	printf("--output_file <output file name>\n");
	printf("--id <hex ID of a new flash image object>\n");
	printf("--major <decimal major version of the object>\n");
	printf("--minor <decimal minor version of the object>\n");
	printf("--fix <decimal fix version of the object>\n");
	printf("--desc <flash image object description>\n");
	printf("--load_addr <hex flash image object load address>\n");
	printf("--exec_addr <hex flash image object load address>\n");
	printf("--flags <hex flash image object flags>\n");
	printf("\n");
}

int main(int argc, char **argv)
{
	struct al_flash_obj_hdr obj_hdr;
	struct al_flash_obj_hdr obj_hdr_le;
	FILE *f;
	int i;
	char *input_file_name = NULL;
	char *input_file_name_stage2_5 = NULL;
	char *input_file_name_stage3 = NULL;
	char *output_file_name = NULL;
	int err = 0;
	uint32_t csum;
	uint32_t size_le;
	int curr_offset = 0;
	uint8_t *buff;
	long file_size;
	uint8_t *buff_stage2_5;
	long file_size_stage2_5;
	uint8_t *buff_stage3;
	long file_size_stage3;

	memset(&obj_hdr, 0, sizeof(struct al_flash_obj_hdr));
	obj_hdr.magic_num = AL_FLASH_OBJ_MAGIC_NUM;
	obj_hdr.format_rev_id = AL_FLASH_OBJ_FORMAT_REV_ID_MIN;
	strncpy(obj_hdr.desc, "N/A", AL_FLASH_OBJ_DESC_LEN);

	if (argc < 2) {
		syntax_err("Invalid number of arguments!\n");
		err = -EINVAL;
		goto done;
	}

	for (i = 1; i < argc; i++) {
		char *arg = argv[i];

		if (!strcmp(arg, "--output_file")) {
			if (i >= (argc - 1)) {
				syntax_err("output file not provided");
				err = -EINVAL;
				goto done;
			}
			i++;
			char *arg = argv[i];

			output_file_name = arg;
		} else if (!strcmp(arg, "--input_file")) {
			if (i >= (argc - 1)) {
				syntax_err("input file not provided");
				err = -EINVAL;
				goto done;
			}
			i++;
			char *arg = argv[i];

			if (!input_file_name) {
				input_file_name = arg;
			} else if (!input_file_name_stage2_5) {
				input_file_name_stage2_5 = arg;
			} else if (!input_file_name_stage3) {
				input_file_name_stage3 = arg;
			} else {
				syntax_err("too many input files provided");
				err = -EINVAL;
				goto done;
			}
		} else if (!strcmp(arg, "--id")) {
			int id;

			if (i >= (argc - 1)) {
				syntax_err("id not provided");
				err = -EINVAL;
				goto done;
			}
			i++;
			char *arg = argv[i];

			id = al_flash_obj_id_from_str(arg);
			if (id >= 0) {
				obj_hdr.id = id;
			} else {
				obj_hdr.id = strtoul(arg, NULL, 16);
			}
		} else if (!strcmp(arg, "--major")) {
			if (i >= (argc - 1)) {
				syntax_err("major not provided");
				err = -EINVAL;
				goto done;
			}
			i++;
			char *arg = argv[i];

			obj_hdr.major_ver = strtoul(arg, NULL, 10);
		} else if (!strcmp(arg, "--minor")) {
			if (i >= (argc - 1)) {
				syntax_err("minor not provided");
				err = -EINVAL;
				goto done;
			}
			i++;
			char *arg = argv[i];

			obj_hdr.minor_ver = strtoul(arg, NULL, 10);
		} else if (!strcmp(arg, "--fix")) {
			if (i >= (argc - 1)) {
				syntax_err("fix not provided");
				err = -EINVAL;
				goto done;
			}
			i++;
			char *arg = argv[i];

			obj_hdr.fix_ver = strtoul(arg, NULL, 10);
		} else if (!strcmp(arg, "--desc")) {
			if (i >= (argc - 1)) {
				syntax_err("description not provided");
				err = -EINVAL;
				goto done;
			}
			i++;
			char *arg = argv[i];

			strncpy(obj_hdr.desc, arg, AL_FLASH_OBJ_DESC_LEN);
		} else if (!strcmp(arg, "--load_addr")) {
			if (i >= (argc - 1)) {
				syntax_err("load address not provided");
				err = -EINVAL;
				goto done;
			}
			i++;
			char *arg = argv[i];

			obj_hdr.load_addr_hi = (uint32_t)(strtoull(arg, NULL, 16) >> 32);
			obj_hdr.load_addr_lo = (uint32_t)(strtoull(arg, NULL, 16) & 0xffffffff);
		} else if (!strcmp(arg, "--exec_addr")) {
			if (i >= (argc - 1)) {
				syntax_err("exec address not provided");
				err = -EINVAL;
				goto done;
			}
			i++;
			char *arg = argv[i];

			obj_hdr.exec_addr_hi = (uint32_t)(strtoull(arg, NULL, 16) >> 32);
			obj_hdr.exec_addr_lo = (uint32_t)(strtoull(arg, NULL, 16) & 0xffffffff);
		} else if (!strcmp(arg, "--flags")) {
			if (i >= (argc - 1)) {
				syntax_err("flags not provided");
				err = -EINVAL;
				goto done;
			}
			i++;
			char *arg = argv[i];

			obj_hdr.flags = strtoul(arg, NULL, 16);
		} else {
			syntax_err("Invalid argument!\n");
			err = -EINVAL;
			goto done;
		}
	}

	if (!input_file_name) {
		syntax_err("input file name not provided");
		err = -EINVAL;
		goto done;
	}

	if (AL_FLASH_OBJ_ID_ID(obj_hdr.id) == AL_FLASH_OBJ_ID_PRE_BOOT) {
		if (!input_file_name_stage2_5) {
			syntax_err("stage 2.5 input file name not provided");
			err = -EINVAL;
			goto done;
		}

		if (!input_file_name_stage3) {
			syntax_err("stage 3 input file name not provided");
			err = -EINVAL;
			goto done;
		}
	}

	if (!output_file_name) {
		syntax_err("output file name not provided");
		err = -EINVAL;
		goto done;
	}

	err = read_file_to_buff(input_file_name, &buff, &file_size);
	if (err)
		goto done;
	obj_hdr.size = file_size;

	if (AL_FLASH_OBJ_ID_ID(obj_hdr.id) == AL_FLASH_OBJ_ID_PRE_BOOT) {
		err = read_file_to_buff(input_file_name_stage2_5, &buff_stage2_5, &file_size_stage2_5);
		if (err)
			goto done;
		err = read_file_to_buff(input_file_name_stage3, &buff_stage3, &file_size_stage3);
		if (err)
			goto done;
		obj_hdr.size = AL_FLASH_PRE_BOOT_HEADER_OFFSET + AL_FLASH_PRE_BOOT_STG3_OFFSET +
			sizeof(uint32_t) + file_size_stage3 - AL_FLASH_PRE_BOOT_STG2_5_OFFSET;
	}

	printf("magic_num = %08x\n", obj_hdr.magic_num);
	printf("format_rev_id = %08x\n", obj_hdr.format_rev_id);
	printf("id = %08x\n", obj_hdr.id);
	printf("major_ver = %08x\n", obj_hdr.major_ver);
	printf("minor_ver = %08x\n", obj_hdr.minor_ver);
	printf("fix_ver = %08x\n", obj_hdr.fix_ver);
	printf("desc = %s\n", obj_hdr.desc);
	printf("size = %08x\n", obj_hdr.size);
	printf("load_addr_hi = %08x\n", obj_hdr.load_addr_hi);
	printf("load_addr_lo = %08x\n", obj_hdr.load_addr_lo);
	printf("exec_addr_hi = %08x\n", obj_hdr.exec_addr_hi);
	printf("exec_addr_lo = %08x\n", obj_hdr.exec_addr_lo);
	printf("flags = %08x\n", obj_hdr.flags);

	memcpy(&obj_hdr_le, &obj_hdr, sizeof(struct al_flash_obj_hdr));
	obj_hdr_le.magic_num = le(obj_hdr.magic_num);
	obj_hdr_le.format_rev_id = le(obj_hdr.format_rev_id);
	obj_hdr_le.id = le(obj_hdr.id);
	obj_hdr_le.major_ver = le(obj_hdr.major_ver);
	obj_hdr_le.minor_ver = le(obj_hdr.minor_ver);
	obj_hdr_le.fix_ver = le(obj_hdr.fix_ver);
	obj_hdr_le.size = le(obj_hdr.size);
	obj_hdr_le.load_addr_hi = le(obj_hdr.load_addr_hi);
	obj_hdr_le.load_addr_lo = le(obj_hdr.load_addr_lo);
	obj_hdr_le.exec_addr_hi = le(obj_hdr.exec_addr_hi);
	obj_hdr_le.exec_addr_lo = le(obj_hdr.exec_addr_lo);
	obj_hdr_le.flags = le(obj_hdr.flags);
	obj_hdr_le.checksum = le(chksum32((uint8_t *)&obj_hdr_le, sizeof(struct al_flash_obj_hdr) - sizeof(uint32_t)));

	f = fopen(output_file_name, "wb");
	if (!f)
		goto done;

	if (AL_FLASH_OBJ_ID_ID(obj_hdr.id) == AL_FLASH_OBJ_ID_STG2) {
		err = write_buff(f, buff, file_size, 0);
		if (err)
			goto done_close_file;

		err = write_buff(f, (uint8_t *)&obj_hdr_le, sizeof(struct al_flash_obj_hdr), 0);
		if (err)
			goto done_close_file;
	} else if (AL_FLASH_OBJ_ID_ID(obj_hdr.id) == AL_FLASH_OBJ_ID_PRE_BOOT) {
		uint32_t csum_temp;

		err = write_buff_get_csum(f, buff, file_size, AL_FLASH_PRE_BOOT_HEADER_OFFSET, &csum_temp);
		if (err)
			goto done_close_file;
		csum = csum_temp;

		err = write_buff(f, (uint8_t *)&obj_hdr_le, sizeof(struct al_flash_obj_hdr),
			AL_FLASH_PRE_BOOT_STG2_5_OFFSET - AL_FLASH_PRE_BOOT_HEADER_OFFSET);
		if (err)
			goto done_close_file;

		size_le = le(file_size_stage2_5);
		err = write_buff_get_csum(f, (uint8_t*)&size_le, sizeof(uint32_t), 0, &csum_temp);
		if (err)
			goto done_close_file;
		csum += csum_temp;
		err = write_buff_get_csum(f, buff_stage2_5, file_size_stage2_5, AL_FLASH_PRE_BOOT_STG3_OFFSET - AL_FLASH_PRE_BOOT_STG2_5_OFFSET - sizeof(uint32_t), &csum_temp);
		if (err)
			goto done_close_file;
		csum += csum_temp;

		size_le = le(file_size_stage3);
		err = write_buff_get_csum(f, (uint8_t*)&size_le, sizeof(uint32_t), 0, &csum_temp);
		if (err)
			goto done_close_file;
		csum += csum_temp;
		err = write_buff_get_csum(f, buff_stage3, file_size_stage3, 0, &csum_temp);
		if (err)
			goto done_close_file;
		csum += csum_temp;

		csum = le(csum);
		err = write_buff(f, (uint8_t *)&csum, sizeof(uint32_t), 0);
		if (err)
			goto done_close_file;
	} else {
		err = write_buff(f, (uint8_t *)&obj_hdr_le, sizeof(struct al_flash_obj_hdr), 0);
		if (err)
			goto done_close_file;

		err = write_buff(f, buff, file_size, 0);
		if (err)
			goto done_close_file;

		csum = le(chksum32((uint8_t *)buff, file_size));
		err = write_buff(f, (uint8_t *)&csum, sizeof(uint32_t), 0);
		if (err)
			goto done_close_file;
	}

done_close_file:
	fclose(f);

done:
	if (err)
		printf("Failed!\n");

	return err;
}

