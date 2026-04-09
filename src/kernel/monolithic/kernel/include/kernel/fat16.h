#ifndef KERNEL_FAT16_H
#define KERNEL_FAT16_H

#include <stdint.h>

#define FAT16_OEM_NAME_SIZE 8
#define FAT16_BOOT_SIGNATURE 0xAA55

typedef struct fat16_fs
{
    uint32_t volume_lba;
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sector_count;
    uint8_t num_fats;
    uint16_t root_entry_count;
    uint16_t total_sectors_16;
    uint32_t total_sectors_32;
    uint16_t sectors_per_fat;
    uint32_t total_sectors;
    uint32_t root_dir_sectors;
    uint32_t fat_start_lba;
    uint32_t root_dir_start_lba;
    uint32_t data_start_lba;
    uint32_t cluster_count;
    char oem_name[FAT16_OEM_NAME_SIZE + 1];
    uint16_t boot_signature;
} fat16_fs_t;

/**
 * Mount a FAT16 volume at the given starting LBA.
 * @param fs Pointer to an allocated fat16_fs_t.
 * @param volume_lba Starting LBA of the FAT16 volume.
 * @return 0 on success, -1 on failure.
 */
int fat16_mount(fat16_fs_t *fs, uint32_t volume_lba);

#endif /* KERNEL_FAT16_H */
