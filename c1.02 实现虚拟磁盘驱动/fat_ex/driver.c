/**
 * @file     driver.c
 * @brief    驱动接口
 * @author   Fanyu Meng
 * @email    mengfanyu0217@163.com
 * @date     2019/9/6
*/

#include <stdio.h>
#include "xdisk.h"
#include "xfat.h"

/**
 * @brief    初始化磁盘设备
 * @param    disk  初始化的设备
 * @param    init_data  设备名称
 * @return   
*/
static xfat_err_t xdisk_hw_open(struct _xdisk_t* disk, void* init_data) {
	const char* path = (const char*)init_data;

	FILE* file = fopen(path, "rb+");
	if (file == NULL) {
		printf("open disk failed: %s\n", path);
		return FS_ERR_IO;
	}

	disk->data = file;
	disk->sector_size = 512;

	fseek(file, 0, SEEK_END);
	disk->total_sector = ftell(file) / disk->sector_size;
	return FS_ERR_OK;
}

/**
 * @brief    关闭磁盘设备
 * @param    disk  要关闭的设备
 * @return
*/
static xfat_err_t xdisk_hw_close(struct _xdisk_t* disk) {
	FILE* file = (FILE*)disk->data;
	fclose(file);
	return FS_ERR_OK;
}

/**
 * @brief    从设备中读取指定扇区数量的数据
 * @param    disk  要读取的磁盘
 * @param    buffer  读取数据存储的缓冲区
 * @param    start_sector  读取的起始扇区
 * @param    count  读取的扇区数量
 * @return
*/
static xfat_err_t xdisk_hw_read_sector(struct _xdisk_t* disk, u8_t* buffer, u32_t start_sector, u32_t count) {
	u32_t offset = start_sector * disk->sector_size;	
	FILE* file = (FILE*)disk->data;
	
	int err = fseek(file, offset, SEEK_SET);
	if (err == -1) {
		printf("seek disk failed: 0x%x\n", offset);
		return FS_ERR_IO;
	}

	err = fread(buffer, disk->sector_size, count, file);
	if (err == -1) {
		printf("read disk failed: sector: %d, count: %d\n", start_sector, count);
		return FS_ERR_IO;
	}

	return FS_ERR_OK;
}
/**
 * @brief    向设备中写入指定扇区数量的数据
 * @param    disk  要写入的磁盘
 * @param    buffer  数据源缓冲区
 * @param    start_sector  写入的起始扇区
 * @param    count  写入的扇区数量
 * @return
*/
static xfat_err_t xdisk_hw_write_sector(struct _xdisk_t* disk, u8_t* buffer, u32_t start_sector, u32_t count) {
	u32_t offset = start_sector * disk->sector_size;
	FILE* file = (FILE*)disk->data;

	int err = fseek(file, offset, SEEK_SET);
	if (err == -1) {
		printf("seek disk failed: 0x%x\n", offset);
		return FS_ERR_IO;
	}

	err = fwrite(buffer, disk->sector_size, count, file);
	if (err == -1) {
		printf("write disk failed: sector: %d, count: %d\n", start_sector, count);
		return FS_ERR_IO;
	}

	fflush(file);
	return FS_ERR_OK;
}

/**
 * @brief    虚拟磁盘驱动结构
*/
xdisk_driver_t vdisk_driver = {
	.open = xdisk_hw_open,
	.close = xdisk_hw_close,
	.read_sector = xdisk_hw_read_sector,
	.write_sector = xdisk_hw_write_sector,
};
