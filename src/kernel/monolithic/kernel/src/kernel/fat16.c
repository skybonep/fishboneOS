#include <stdint.h>
#include <string.h>

#include <kernel/log.h>
#include <kernel/fat16.h>
#include <drivers/block.h>

static uint16_t fat16_read_le16(const uint8_t *buffer)
{
    return (uint16_t)buffer[0] | ((uint16_t)buffer[1] << 8);
}

static uint32_t fat16_read_le32(const uint8_t *buffer)
{
    return (uint32_t)buffer[0] |
           ((uint32_t)buffer[1] << 8) |
           ((uint32_t)buffer[2] << 16) |
           ((uint32_t)buffer[3] << 24);
}

int fat16_mount(fat16_fs_t *fs, uint32_t volume_lba)
{
    if (fs == NULL)
        return -1;

    uint8_t sector[512];
    if (block_read(volume_lba, sector, 1) < 0)
    {
        printk(LOG_ERROR, "FAT16: failed to read boot sector at LBA %u", volume_lba);
        return -1;
    }

    uint16_t signature = fat16_read_le16(&sector[510]);
    if (signature != FAT16_BOOT_SIGNATURE)
    {
        printk(LOG_ERROR, "FAT16: invalid boot signature 0x%04x", signature);
        return -1;
    }

    memset(fs, 0, sizeof(*fs));
    fs->volume_lba = volume_lba;
    fs->bytes_per_sector = fat16_read_le16(&sector[0x0B]);
    fs->sectors_per_cluster = sector[0x0D];
    fs->reserved_sector_count = fat16_read_le16(&sector[0x0E]);
    fs->num_fats = sector[0x10];
    fs->root_entry_count = fat16_read_le16(&sector[0x11]);
    fs->total_sectors_16 = fat16_read_le16(&sector[0x13]);
    fs->sectors_per_fat = fat16_read_le16(&sector[0x16]);
    fs->total_sectors_32 = fat16_read_le32(&sector[0x20]);
    fs->boot_signature = signature;

    for (uint32_t i = 0; i < FAT16_OEM_NAME_SIZE; ++i)
    {
        fs->oem_name[i] = sector[0x03 + i];
    }
    fs->oem_name[FAT16_OEM_NAME_SIZE] = '\0';

    if (fs->bytes_per_sector == 0 || fs->sectors_per_cluster == 0 || fs->reserved_sector_count == 0 ||
        fs->num_fats == 0 || fs->root_entry_count == 0 || fs->sectors_per_fat == 0)
    {
        printk(LOG_ERROR, "FAT16: unsupported or corrupt BPB");
        return -1;
    }

    fs->total_sectors = (fs->total_sectors_16 != 0) ? fs->total_sectors_16 : fs->total_sectors_32;
    if (fs->total_sectors == 0)
    {
        printk(LOG_ERROR, "FAT16: total sector count is zero");
        return -1;
    }

    fs->root_dir_sectors = ((fs->root_entry_count * 32U) + (fs->bytes_per_sector - 1U)) / fs->bytes_per_sector;
    fs->fat_start_lba = fs->volume_lba + fs->reserved_sector_count;
    fs->root_dir_start_lba = fs->fat_start_lba + (fs->num_fats * (uint32_t)fs->sectors_per_fat);
    fs->data_start_lba = fs->root_dir_start_lba + fs->root_dir_sectors;

    uint32_t data_sectors = fs->total_sectors - (fs->reserved_sector_count + fs->num_fats * (uint32_t)fs->sectors_per_fat + fs->root_dir_sectors);
    fs->cluster_count = data_sectors / fs->sectors_per_cluster;

    printk(LOG_INFO, "FAT16: mounted volumeLBA=%u oem='%s' bytes=%u clusters=%u fatStart=%u rootStart=%u dataStart=%u",
           fs->volume_lba,
           fs->oem_name,
           fs->bytes_per_sector,
           fs->cluster_count,
           fs->fat_start_lba,
           fs->root_dir_start_lba,
           fs->data_start_lba);

    return 0;
}
