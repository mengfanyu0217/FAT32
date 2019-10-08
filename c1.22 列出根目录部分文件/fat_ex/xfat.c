#include "xfat.h"
#include "xdisk.h"

extern u8_t temp_buffer[512];

#define xfat_get_disk(xfat)     ((xfat)->disk_part->disk)

static xfat_err_t parse_fat_header(xfat_t* xfat, dbr_t* dbr) {
	xdisk_part_t* xdisk_part = xfat->disk_part;
	
	xfat->root_cluster = dbr->fat32.BPB_RootClus;
	xfat->fat_tbl_sectors = dbr->fat32.BPB_FATSz32;
	
	if (dbr->fat32.BPB_ExtFlags & (1 << 7)) {
		u32_t table = dbr->fat32.BPB_ExtFlags & 0xF;
		xfat->fat_start_sector = dbr->bpb.BPB_RsvdSecCnt + xdisk_part->start_sector + table * xfat->fat_tbl_sectors;
		xfat->fat_tbl_nr = 1;
	}
	else {
		xfat->fat_start_sector = dbr->bpb.BPB_RsvdSecCnt + xdisk_part->start_sector;
		xfat->fat_tbl_nr = dbr->bpb.BPB_NumFATs;	
	}

	xfat->total_sectors = dbr->bpb.BPB_TotSec32;

	return FS_ERR_OK;
}

xfat_err_t xfat_open(xfat_t* xfat, xdisk_part_t* xdisk_part) {
	dbr_t* dbr = (dbr_t*)temp_buffer;
	xdisk_t* xdisk = xdisk_part->disk;
	xfat_err_t err;
	xfat->disk_part = xdisk_part;

	err = xdisk_read_sector(xdisk, (u8_t*)dbr, xdisk_part->start_sector, 1);
	if (err < 0)
		return err;

	err = parse_fat_header(xfat, dbr);
	if (err < 0)
		return err;
	
	xfat->sec_per_cluster = dbr->bpb.BPB_SecPerClus;
	xfat->total_sectors = dbr->bpb.BPB_TotSec32;
	xfat->cluster_byte_size = xfat->sec_per_cluster * dbr->bpb.BPB_BytsPerSec;

	return FS_ERR_OK;
}

u32_t cluster_fist_sector(xfat_t* xfat, u32_t cluster_no) {
	u32_t data_start_sector = xfat->fat_start_sector + xfat->fat_tbl_sectors * xfat->fat_tbl_nr;
	return data_start_sector + (cluster_no - 2) * xfat->sec_per_cluster;
}

xfat_err_t read_cluster(xfat_t* xfat, u8_t* buffer, u32_t cluster, u32_t count) {
	xfat_err_t err = 0;
	u8_t* curr_buffer = buffer;
	u32_t curr_sector = cluster_fist_sector(xfat, cluster);

	for (u32_t i = 0; i < count; i++) {
		err = xdisk_read_sector(xfat_get_disk(xfat), curr_buffer, curr_sector, xfat->sec_per_cluster);
		if (err < 0)
			return err;

		curr_buffer += xfat->cluster_byte_size;
		curr_sector += xfat->sec_per_cluster;	
	}
	
	return FS_ERR_OK;
}
