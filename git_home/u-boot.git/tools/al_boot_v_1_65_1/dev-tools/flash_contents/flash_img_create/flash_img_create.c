#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "al_flash_contents.h"

#define INVALID		0xffffffff

#define SZ_1K		1024

#define TEMP_BUFF_SIZE	SZ_1K

static uint8_t temp_buff[TEMP_BUFF_SIZE];

static uint32_t chksum32(uint8_t *buff, int len)
{
	uint32_t val = 0;

	for (; len; len--, buff++) {
		val += *buff;
	}

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

//	printf("%s(%p, %p, %08x, %08x)\n", __func__, f, buff, buff_size, padded_size);

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

void syntax_err(int obj_idx, char *mess)
{
	if (obj_idx > -1)
		printf("Syntax error (obj %d): %s\n\n", obj_idx, mess);
	else
		printf("Syntax error: %s\n\n", mess);

	printf("Syntax: flash_img_create [options]\n\n");
	printf("--output_file <output file name>\n");
	printf("--toc_offset <TOC offset in flash image>\n");
	printf("--id <hex ID of a new flash image object>\n");
	printf("--instance <flash image instance number>\n");
	printf("--name <flash image object name>\n");
	printf("--dev <flash image object device: 0 - current, 1 - SPI, 2 - NAND, 3 - DRAM>\n");
	printf("--offset <hex flash image object offset in the device>\n");
	printf("--upto <hex flash image object padding offset in the device\n");
	printf("--flags <hex flash image object flags>\n");
	printf("--file <flash image object input file>\n");
	printf("\n");
}

int main(int argc, char **argv)
{
	struct al_flash_toc_hdr toc_hdr;
	struct al_flash_toc_entry entries[AL_FLASH_TOC_MAX_NUM_ENTRIES];
	struct al_flash_toc_entry entries_le[AL_FLASH_TOC_MAX_NUM_ENTRIES];
	char *entries_files[AL_FLASH_TOC_MAX_NUM_ENTRIES] = { NULL };
	int last_entry_with_file = -1;
	int in_entry = 0;
	int num_entries = 0;
	unsigned int toc_offset = 12 * SZ_1K;
	FILE *f;
	int i;
	char *output_file_name = NULL;
	int err = 0;
	uint32_t csum;
	int toc_written = 0;
	int curr_offset = 0;

	if (argc < 2) {
		syntax_err(-1, "Invalid number of arguments!\n");
		err = -EINVAL;
		goto done;
	}

	for (i = 1; i < argc; i++) {
		char *arg = argv[i];

		if (!strcmp(arg, "--output_file")) {
			if (i >= (argc - 1)) {
				syntax_err(-1, "output file not provided");
				err = -EINVAL;
				goto done;
			}
			i++;
			char *arg = argv[i];

			output_file_name = arg;
		} else if (!strcmp(arg, "--id")) {
			int id;
			
			if (i >= (argc - 1)) {
				syntax_err(num_entries, "id not provided");
				err = -EINVAL;
				goto done;
			}
			i++;
			char *arg = argv[i];

			if (in_entry) {
				if (entries[num_entries].offset == INVALID) {
					syntax_err(num_entries, "offset not provided");
					err = -EINVAL;
					goto done;
				}

				num_entries++;
			} else
				in_entry = 1;

			memset(&entries[num_entries], 0, sizeof(struct al_flash_toc_entry));

			id = al_flash_obj_id_from_str(arg);
			if (id >= 0) {
				entries[num_entries].obj_id = id;
			} else {
				entries[num_entries].obj_id = strtoul(arg, NULL, 16);
			}

			strncpy(entries[num_entries].obj_id_str, "N/A", AL_FLASH_TOC_ENTRY_OBJ_ID_STR_LEN);
			entries[num_entries].dev_id = AL_FLASH_DEV_ID_CURRENT;
			entries[num_entries].offset = INVALID;
			entries[num_entries].max_size = INVALID;
			entries[num_entries].flags = 0;
		} else if (!strcmp(arg, "--instance")) {
			if (i >= (argc - 1)) {
				syntax_err(num_entries, "instance not provided");
				err = -EINVAL;
				goto done;
			}
			i++;
			char *arg = argv[i];

			entries[num_entries].obj_id  = AL_FLASH_OBJ_ID(AL_FLASH_OBJ_ID_ID(entries[num_entries].obj_id ), strtoul(arg, NULL, 16));
		} else if (!strcmp(arg, "--name")) {
			if (i >= (argc - 1)) {
				syntax_err(num_entries, "name not provided");
				err = -EINVAL;
				goto done;
			}
			i++;
			char *arg = argv[i];

			strncpy(entries[num_entries].obj_id_str, arg, AL_FLASH_TOC_ENTRY_OBJ_ID_STR_LEN);
		} else if (!strcmp(arg, "--dev")) {
			if (i >= (argc - 1)) {
				syntax_err(num_entries, "device not provided");
				err = -EINVAL;
				goto done;
			}
			i++;
			char *arg = argv[i];

			entries[num_entries].dev_id = strtoul(arg, NULL, 16);
		} else if (!strcmp(arg, "--offset")) {
			if (i >= (argc - 1)) {
				syntax_err(num_entries, "offset not provided");
				err = -EINVAL;
				goto done;
			}
			i++;
			char *arg = argv[i];

			entries[num_entries].offset = strtoul(arg, NULL, 16);

			if ((num_entries > 0) && (entries[num_entries - 1].max_size == INVALID))
				entries[num_entries - 1].max_size = entries[num_entries].offset - entries[num_entries - 1].offset;
		} else if (!strcmp(arg, "--upto")) {
			if (i >= (argc - 1)) {
				syntax_err(num_entries, "limit not provided");
				err = -EINVAL;
				goto done;
			}
			i++;
			char *arg = argv[i];

			if (entries[num_entries].offset == INVALID) {
				syntax_err(num_entries, "offset not provided");
				err = -EINVAL;
				goto done;
			}

			entries[num_entries].max_size =	strtoul(arg, NULL, 16) - entries[num_entries].offset;
		} else if (!strcmp(arg, "--flags")) {
			if (i >= (argc - 1)) {
				syntax_err(num_entries, "flags not provided");
				err = -EINVAL;
				goto done;
			}
			i++;
			char *arg = argv[i];

			entries[num_entries].flags = strtoul(arg, NULL, 16);
		} else if (!strcmp(arg, "--file")) {
			if (i >= (argc - 1)) {
				syntax_err(num_entries, "file not provided");
				err = -EINVAL;
				goto done;
			}
			i++;
			char *arg = argv[i];

			entries_files[num_entries] = arg;
			last_entry_with_file = num_entries;
		} else if (!strcmp(arg, "--toc_offset")) {
			if (i >= (argc - 1)) {
				syntax_err(-1, "TOC offset not provided");
				err = -EINVAL;
				goto done;
			}
			i++;
			char *arg = argv[i];

			toc_offset = strtoul(arg, NULL, 16);
		} else {
			syntax_err(-1, "Invalid argument!\n");
			err = -EINVAL;
			goto done;
		}
	}

	if (in_entry) {
		if ((entries[num_entries].offset == INVALID) || (entries[num_entries].max_size == INVALID)) {
			syntax_err(num_entries, "offset or limit not provided");
			err = -EINVAL;
			goto done;
		}

		num_entries++;
	}

	if (!num_entries) {
		syntax_err(-1, "no input entries provided");
		err = -EINVAL;
		goto done;
	}

	if (!output_file_name) {
		syntax_err(-1, "output file name not provided");
		err = -EINVAL;
		goto done;
	}

	toc_hdr.magic_num = le(AL_FLASH_TOC_MAGIC_NUM);
	toc_hdr.format_rev_id = le(AL_FLASH_TOC_FORMAT_REV_ID_MIN);
	toc_hdr.num_entries = le(num_entries);
	toc_hdr.checksum = le(chksum32((uint8_t *)&toc_hdr, sizeof(struct al_flash_toc_hdr) - sizeof(uint32_t)));

	f = fopen(output_file_name, "wb");

	for (i = 0; i < num_entries; i++) {
		memcpy(&entries_le[i], &entries[i], sizeof(struct al_flash_toc_entry));
		entries_le[i].obj_id = le(entries_le[i].obj_id);
		entries_le[i].dev_id = le(entries_le[i].dev_id);
		entries_le[i].offset = le(entries_le[i].offset);
		entries_le[i].max_size = le(entries_le[i].max_size);
		entries_le[i].flags = le(entries_le[i].flags);
	}

	printf("TOC offset = %08x\n", toc_offset);

	for (i = 0; i < num_entries; i++) {
		printf("\tobj %d obj_id = %08x\n", i, entries[i].obj_id);
		printf("\tobj %d obj_id_str = %s\n", i, entries[i].obj_id_str);
		printf("\tobj %d dev_id = %08x\n", i, entries[i].dev_id);
		printf("\tobj %d offset = %08x\n", i, entries[i].offset);
		printf("\tobj %d max_size = %08x\n", i, entries[i].max_size);
		printf("\tobj %d flags = %08x\n", i, entries[i].flags);
	}

	for (i = 0; (i < num_entries) || (!toc_written);) {
		struct al_flash_toc_entry *entry = &entries[i];
		uint8_t *buff;
		long file_size;

		if ((entry->offset > toc_offset) && (!toc_written)) {
			printf("Writing TOC at %08x\n", toc_offset);

			if (curr_offset > toc_offset) {
				printf("TOC overlaps flash object %d!\n", i);
				err = -EINVAL;
				goto done_close_file;
			}

			err = write_buff(f, NULL, 0, toc_offset - curr_offset);
			if (err)
				goto done_close_file;
			curr_offset = toc_offset;
//	printf("curr_offset = %08x\n", curr_offset);

			err = write_buff(f, (uint8_t *)&toc_hdr, sizeof(struct al_flash_toc_hdr), 0);
			if (err)
				goto done_close_file;
			curr_offset += sizeof(struct al_flash_toc_hdr);
//	printf("curr_offset = %08x\n", curr_offset);

			err = write_buff(f, (uint8_t *)entries_le, num_entries * sizeof(struct al_flash_toc_entry), 0);
			if (err)
				goto done_close_file;
			curr_offset += num_entries * sizeof(struct al_flash_toc_entry);
//	printf("curr_offset = %08x\n", curr_offset);

			csum = le(chksum32((uint8_t *)entries_le, num_entries * sizeof(struct al_flash_toc_entry)));
			err = write_buff(f, (uint8_t *)&csum, sizeof(uint32_t), 0);
			if (err)
				goto done_close_file;
			curr_offset += sizeof(uint32_t);
//	printf("curr_offset = %08x\n", curr_offset);

			toc_written = 1;
		}

		if (i < num_entries) {
			printf("Writing object %d at %08x\n", i, entry->offset);

			if (curr_offset >  entry->offset) {
				printf("Object %d overlaps previous object or TOC!\n", i);
				err = -EINVAL;
				goto done_close_file;
			}

			if (i <= last_entry_with_file) {
				err = write_buff(f, NULL, 0, entry->offset - curr_offset);
				if (err)
					goto done_close_file;
				curr_offset = entry->offset;
//	printf("curr_offset = %08x\n", curr_offset);
			}

			if (entries_files[i]) {
				err = read_file_to_buff(entries_files[i], &buff, &file_size);
				if (err)
					goto done_close_file;

				if (file_size > entry->max_size) {
					printf("Object %d exceeds its maximal size!\n", i);
					err = -EINVAL;
					goto done_close_file;
				}

				err = write_buff(f, buff, file_size, 0);
				if (err)
					goto done_close_file;
				curr_offset += file_size;
//				printf("curr_offset = %08x\n", curr_offset);

				free(buff);
			}

			i++;
		}
	}

	if (entries_files[num_entries - 1]) {
		err = write_buff(f, NULL, 0, entries[num_entries - 1].offset + entries[num_entries - 1].max_size - curr_offset);
		if (err)
			goto done_close_file;
	}

done_close_file:
	fclose(f);

done:
	if (err)
		printf("Failed!\n");

	return err ? 1 : 0;
}

