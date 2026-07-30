#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "al_flash_contents.h"

#define SZ_1K		1024

#define TEMP_BUFF_SIZE	SZ_1K

static uint8_t temp_buff[TEMP_BUFF_SIZE];
static uint8_t *mem_buff;
static long mem_buff_size;

static int flash_contents_mem_read(unsigned int offset, void *buff, unsigned int size)
{
	unsigned int num_valid_bytes =
		(offset > mem_buff_size) ? 0 :
		(((offset + size) > mem_buff_size) ?
		(mem_buff_size - offset) :
		size);

	if (num_valid_bytes)
		memcpy(buff, &mem_buff[offset], num_valid_bytes);
	if (size > num_valid_bytes)
		memset(buff + num_valid_bytes, 0xff, size - num_valid_bytes);

	return 0;
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

void syntax_err(char *mess)
{
	printf("Syntax error: %s\n\n", mess);

	printf("Syntax: flash_img_print [options]\n\n");
	printf("--full_img <full image file name>\n");
	printf("--toc_offset <full image TOC offset>\n");
	printf("--obj_img <object image file name>\n");
	printf("\n");
}

int main(int argc, char **argv)
{
	char *full_img_file_name = NULL;
	unsigned int toc_offset = 0;
	char *obj_img_file_name = NULL;
	int obj_is_stage_2 = 0;
	FILE *f;
	int i;
	int err = 0;

	if (argc < 2) {
		syntax_err("Invalid number of arguments!\n");
		err = -EINVAL;
		goto done;
	}

	for (i = 1; i < argc; i++) {
		char *arg = argv[i];

		if (!strcmp(arg, "--full_img")) {
			if (i >= (argc - 1)) {
				syntax_err("full image file name not provided");
				err = -EINVAL;
				goto done;
			}
			i++;
			char *arg = argv[i];

			full_img_file_name = arg;
		} else if (!strcmp(arg, "--toc_offset")) {
			if (i >= (argc - 1)) {
				syntax_err("TOC offset not provided");
				err = -EINVAL;
				goto done;
			}
			i++;
			char *arg = argv[i];

			toc_offset = strtoul(arg, NULL, 16);
		} else if (!strcmp(arg, "--obj_img")) {
			if (i >= (argc - 1)) {
				syntax_err("full image file name not provided");
				err = -EINVAL;
				goto done;
			}
			i++;
			char *arg = argv[i];

			obj_img_file_name = arg;
		} else {
			syntax_err("Invalid argument!\n");
			err = -EINVAL;
			goto done;
		}
	}

	if (full_img_file_name) {
		unsigned int toc_total_size;
		unsigned int toc_num_entries;

		f = fopen(full_img_file_name, "rb");
		if (!f) {
			printf("Unable to open full image file!\n");
			err = -EIO;
			goto done;
		}

		err = read_file_to_buff(full_img_file_name, &mem_buff, &mem_buff_size);
		if (err) {
			printf("Unable to read full image file to buffer!\n");
			fclose(f);
			goto done;
		}

		err = al_flash_toc_validate(flash_contents_mem_read, toc_offset,
			&toc_total_size, &toc_num_entries);
		if (err) {
			printf("Full image validation failed!\n");
			goto done;
		}

		al_flash_toc_print(flash_contents_mem_read, toc_offset);

		for (i = 0; i < toc_num_entries; i++) {
			struct al_flash_toc_entry toc_entry;
			err = al_flash_toc_entry_get(flash_contents_mem_read, toc_offset, i, &toc_entry);
			if (err) {
				printf("al_flash_toc_entry_get failed!\n");
				goto done;
			}

			printf("-------------------------------------\n");
			al_flash_obj_info_print(flash_contents_mem_read, toc_entry.offset, AL_FLASH_OBJ_ID_ID(toc_entry.obj_id), temp_buff, TEMP_BUFF_SIZE);
		}

		free(mem_buff);
		fclose(f);
	}

	if (obj_img_file_name) {
		f = fopen(obj_img_file_name, "rb");
		if (!f) {
			printf("Unable to open object image file!\n");
			err = -EIO;
			goto done;
		}

		err = read_file_to_buff(obj_img_file_name, &mem_buff, &mem_buff_size);
		if (err) {
			printf("Unable to read object image file to buffer!\n");
			fclose(f);
			goto done;
		}

		al_flash_obj_info_print(flash_contents_mem_read, 0, obj_is_stage_2 ? AL_FLASH_OBJ_ID_STG2 : AL_FLASH_OBJ_ID_BOOT_MODE, temp_buff, TEMP_BUFF_SIZE);

		free(mem_buff);
		fclose(f);
	}

done:
	return err;
}

