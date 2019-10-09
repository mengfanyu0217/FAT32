#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "xdisk.h"
#include "xfat.h"

extern xdisk_driver_t vdisk_driver;

const char* disk_path_test = "disk_test.vhd";
const char* disk_path = "disk.vhd";

static u32_t write_buffer[160 * 1024];
static u32_t read_buffer[160 * 1024];

xdisk_t disk;
xdisk_part_t disk_part;

xfat_t xfat;

/**
 * @brief    ����IO���ԣ�������ɺ���رգ�
 * @return
*/
int disk_io_test(void) {
	int err;

	xdisk_t disk_test;

	memset(read_buffer, 0, sizeof(read_buffer));

	err = xdisk_open(&disk_test, "vdisk_test", &vdisk_driver, (void*)disk_path_test);
	if (err) {
		printf("open disk failed!\n");
		return -1;
	}

	err = xdisk_write_sector(&disk_test, (u8_t*)write_buffer, 0, 2);
	if (err) {
		printf("write disk failed!\n");
		return -1;
	}

	err = xdisk_read_sector(&disk_test, (u8_t*)read_buffer, 0, 2);
	if (err) {
		printf("read disk failed!\n");
		return -1;
	}

	err = memcmp((u8_t*)read_buffer, (u8_t*)write_buffer, disk_test.sector_size * 2);
	if (err != 0) {
		printf("data is not equal!\n");
		return -1;
	}

	err = xdisk_close(&disk_test);
	if (err) {
		printf("close disk failed!\n");
		return -1;
	}

	printf("disk io test is OK!\n");
	return 0;
}

/**
 * @brief    ��������
 * @return
*/
int disk_part_test(void) {
	u32_t count;
	xfat_err_t err = FS_ERR_OK;

	printf("partition read test...\n");

	err = xdisk_get_part_count(&disk, &count);
	if (err < 0) {
		printf("partition count detect failed!\n");
		return err;
	}

	for (u32_t i = 0; i < count; i++) {
		xdisk_part_t part;

		int err = xdisk_get_part(&disk, &part, i);
		if (err < 0) {
			printf("read partition failed!\n");
			return -1;
		}
	
		printf("no: %d, start: %d, count: %d, capacity: %.0f M\n", i, part.start_sector, part.total_sector, part.total_sector * disk.sector_size / 1024 / 1024.0);
	}

	printf("partition count: %d\n", count);
	return 0;
}

void show_dir_info(diritem_t * diritem) {
	char file_name[12];
	u8_t attr = diritem->DIR_Attr;

	memset(file_name, 0, sizeof(file_name));
	memcpy(file_name, diritem->DIR_Name, 11);
	if (file_name[0] == 0x05)
		file_name[0] = 0xE5;

	printf("\n name: %s, ", file_name);

	printf("\n\t");

	if (attr & DIRITEM_ATTR_READ_ONLY)
		printf("read only, ");

	if (attr & DIRITEM_ATTR_HIDDEN)
		printf("hidden, ");

	if (attr & DIRITEM_ATTR_SYSTEM)
		printf("system, ");

	if (attr & DIRITEM_ATTR_DIRECTORY)
		printf("directory, ");

	if (attr & DIRITEM_ATTR_ARCHIVE)
		printf("archive, ");

	printf("\n\t create: %d-%d-%d, ", diritem->DIR_CrtDate.year_from_1980 + 1980, 
		diritem->DIR_CrtDate.month, diritem->DIR_CrtDate.day);
	printf("\n\t time: %d-%d-%d, ", diritem->DIR_CrtTime.hour, diritem->DIR_CrtTime.minute, 
		diritem->DIR_CrtTime.second_2 * 2 + diritem->DIR_CrtTimeTeenth / 100);

	printf("\n\t last write: %d-%d-%d, ", diritem->DIR_WrtDate.year_from_1980 + 1980,
		diritem->DIR_WrtDate.month, diritem->DIR_WrtDate.day);
	printf("\n\t time: %d-%d-%d, ", diritem->DIR_WrtTime.hour, diritem->DIR_WrtTime.minute,
		diritem->DIR_WrtTime.second_2 * 2);

	printf("\n\t last access: %d-%d-%d, ", diritem->DIR_LastAccDate.year_from_1980 + 1980,
		diritem->DIR_LastAccDate.month, diritem->DIR_LastAccDate.day);

	printf("\n\t size %d KB, ", diritem->DIR_FileSize / 1024);
	printf("\n\t cluster %d, ", (diritem->DIR_FstClusHI << 16) | diritem->DIR_FstClusL0);

	printf("\n");
}

/**
 * @brief    FATĿ¼����
 * @return
*/
int fat_dir_test(void) {
	int index = 0;
	u8_t* cluster_buffer = (u8_t*)malloc(xfat.cluster_byte_size);
	u32_t curr_cluster = xfat.root_cluster;

	while (is_cluster_valid(curr_cluster)) {
		int err = read_cluster(&xfat, cluster_buffer, curr_cluster, 1);
		if (err) {
			printf("read cluster %d failed!\n", curr_cluster);
			return -1;
		}

		diritem_t* dir_item = (diritem_t*)cluster_buffer;
		for (u32_t i = 0; i < xfat.cluster_byte_size / sizeof(diritem_t); i++) {
			u8_t* name = (u8_t*)(dir_item[i].DIR_Name);
			if (name[0] == DIRITEM_NAME_FREE)
				continue;
			else if (name[0] == DIRITEM_NAME_END)
				break;

			++index;
			printf("no: %d, ", index);
			show_dir_info(&dir_item[i]);
		}
		
		err = get_next_cluster(&xfat, curr_cluster, &curr_cluster);
		if (err) {
			printf("get next cluster failed! current cluster: %d\n", curr_cluster);
			return -1;		
		}
	}
	
	return 0;
}

/**
 * @brief    ��ӡ�ļ����ݲ���
 * @return
*/
int fat_file_test(void) {
	u8_t* cluster_buffer = (u8_t*)malloc(xfat.cluster_byte_size + 1);
	u32_t curr_cluster = 4565;
	int size = 0;

	while (is_cluster_valid(curr_cluster)) {
		int err = read_cluster(&xfat, cluster_buffer, curr_cluster, 1);
		if (err) {
			printf("read cluster %d failed!\n", curr_cluster);
			return -1;
		}

		cluster_buffer[xfat.cluster_byte_size + 1] = '\0';
		printf("%s", (char*)cluster_buffer);
		size += xfat.cluster_byte_size;


		err = get_next_cluster(&xfat, curr_cluster, &curr_cluster);
		if (err) {
			printf("get next cluster failed! current cluster: %d\n", curr_cluster);
			return -1;
		}
	}

	printf("\n file size: %d\n", size);
	return 0;
}

int main(void) {
	xfat_err_t err;
	
	for (int i = 0; i < sizeof(write_buffer) / sizeof(u32_t); i++) {
		write_buffer[i] = i;	
	}

	//err = disk_io_test();
	//if (err)
		//return err;

	err = xdisk_open(&disk, "vdisk", &vdisk_driver, (void*)disk_path);
	if (err) {
		printf("open disk failed!\n");
		return -1;
	}

	err = disk_part_test();
	if (err)
		return err;

	err = xdisk_get_part(&disk, &disk_part, 1);
	if (err < 0) {
		printf("read partition failed!\n");
		return -1;
	}

	err = xfat_open(&xfat, &disk_part);
	if (err < 0)
		return err;

	err = fat_dir_test();
	if (err)
		return err;

	err = fat_file_test();
	if (err)
		return err;

	err = xdisk_close(&disk);
	if (err) {
		printf("close disk failed!\n");
		return -1;
	}

	printf("Test is end!\n");
	return 0;
}
