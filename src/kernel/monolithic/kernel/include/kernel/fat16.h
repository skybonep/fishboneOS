#ifndef KERNEL_FAT16_H
#define KERNEL_FAT16_H

#include <stdint.h>

#define FAT16_OEM_NAME_SIZE 8
#define FAT16_BOOT_SIGNATURE 0xAA55
#define FAT16_ATTR_LONG_NAME 0x0F
#define FAT16_ATTR_DIRECTORY 0x10
#define FAT16_ATTR_VOLUME_ID 0x08
#define FAT16_MAX_OPEN_FILES 16
#define FAT16_FD_BASE 3
#define FAT16_MAX_FILE_NAME 11

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

/**
 * Open a root-directory FAT16 file using a short 8.3 name.
 * Returns a FAT16 file descriptor (>= FAT16_FD_BASE) or -1 on failure.
 */
int fat16_open(fat16_fs_t *fs, const char *path);

/**
 * Read from an open FAT16 file descriptor.
 * @param fd FAT16 file descriptor returned by fat16_open.
 * @param buffer Destination buffer.
 * @param count Number of bytes to read.
 * @return Number of bytes read, 0 on EOF, or -1 on failure.
 */
int fat16_read(int fd, uint8_t *buffer, uint32_t count);

/**
 * Create a new root-directory FAT16 file using a short 8.3 name.
 * @param fs Pointer to an allocated fat16_fs_t.
 * @param path File name in the root directory.
 * @return 0 on success, -1 on failure.
 */
int fat16_create(fat16_fs_t *fs, const char *path);

/**
 * Write to an open FAT16 file descriptor.
 * @param fd FAT16 file descriptor returned by fat16_open.
 * @param buffer Source buffer.
 * @param count Number of bytes to write.
 * @return Number of bytes written, or -1 on failure.
 */
int fat16_write(int fd, const uint8_t *buffer, uint32_t count);

/**
 * Close an open FAT16 file descriptor.
 * @param fd FAT16 file descriptor returned by fat16_open.
 * @return 0 on success, -1 on failure.
 */
int fat16_close(int fd);

#endif /* KERNEL_FAT16_H */
