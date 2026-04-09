#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <kernel/log.h>
#include <kernel/fat16.h>
#include <drivers/block.h>

#define FAT16_ENTRY_SIZE 32
#define FAT16_CLUSTER_FREE 0x0000
#define FAT16_CLUSTER_BAD 0xFFF7
#define FAT16_CLUSTER_MIN 0x0002
#define FAT16_CLUSTER_EOC 0xFFF8

typedef struct fat16_file
{
    const fat16_fs_t *fs;
    uint16_t start_cluster;
    uint16_t current_cluster;
    uint32_t size;
    uint32_t position;
    uint8_t active;
} fat16_file_t;

static fat16_file_t fat16_files[FAT16_MAX_OPEN_FILES];
static fat16_fs_t fat16_mounted_fs;
static fat16_fs_t *fat16_current_fs = NULL;

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

static void *fat16_memcpy(void *dest, const void *src, size_t len)
{
    uint8_t *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;
    while (len-- > 0)
    {
        *d++ = *s++;
    }
    return dest;
}

static int fat16_memcmp(const void *a, const void *b, size_t len)
{
    const uint8_t *pa = (const uint8_t *)a;
    const uint8_t *pb = (const uint8_t *)b;
    while (len-- > 0)
    {
        if (*pa != *pb)
        {
            return (int)*pa - (int)*pb;
        }
        pa++;
        pb++;
    }
    return 0;
}

static uint32_t fat16_cluster_to_lba(const fat16_fs_t *fs, uint16_t cluster)
{
    return fs->data_start_lba + ((uint32_t)(cluster - 2) * fs->sectors_per_cluster);
}

static int fat16_is_eoc(uint16_t cluster)
{
    return cluster >= FAT16_CLUSTER_EOC;
}

static char fat16_toupper(char ch)
{
    if (ch >= 'a' && ch <= 'z')
    {
        return ch - ('a' - 'A');
    }
    return ch;
}

static int fat16_normalize_short_name(const char *path, uint8_t normalized[FAT16_MAX_FILE_NAME])
{
    if (path == NULL)
    {
        return -1;
    }

    while (*path == '/')
    {
        path++;
    }

    for (uint32_t i = 0; i < FAT16_MAX_FILE_NAME; ++i)
    {
        normalized[i] = ' ';
    }

    uint32_t name_index = 0;
    uint32_t ext_index = 0;
    int in_extension = 0;

    for (; *path != '\0'; ++path)
    {
        if (*path == '.')
        {
            if (in_extension)
            {
                return -1;
            }
            in_extension = 1;
            continue;
        }

        if (!in_extension)
        {
            if (name_index >= 8)
            {
                return -1;
            }
            normalized[name_index++] = fat16_toupper(*path);
        }
        else
        {
            if (ext_index >= 3)
            {
                return -1;
            }
            normalized[8 + ext_index++] = fat16_toupper(*path);
        }
    }

    if (name_index == 0)
    {
        return -1;
    }

    return 0;
}

static uint16_t fat16_read_fat_entry(const fat16_fs_t *fs, uint16_t cluster)
{
    uint8_t sector[512];
    uint32_t fat_offset = (uint32_t)cluster * 2U;
    uint32_t fat_sector = fs->fat_start_lba + (fat_offset / fs->bytes_per_sector);
    uint32_t fat_index = fat_offset % fs->bytes_per_sector;

    if (block_read(fat_sector, sector, 1) < 0)
    {
        return FAT16_CLUSTER_BAD;
    }

    return fat16_read_le16(&sector[fat_index]);
}

static int fat16_find_root_entry(const fat16_fs_t *fs, const uint8_t target_name[FAT16_MAX_FILE_NAME], uint16_t *out_cluster, uint32_t *out_size)
{
    uint8_t sector[512];
    uint32_t root_sector_count = fs->root_dir_sectors;

    for (uint32_t sector_index = 0; sector_index < root_sector_count; ++sector_index)
    {
        uint32_t lba = fs->root_dir_start_lba + sector_index;
        if (block_read(lba, sector, 1) < 0)
        {
            return -1;
        }

        for (uint32_t offset = 0; offset < fs->bytes_per_sector; offset += FAT16_ENTRY_SIZE)
        {
            uint8_t first_byte = sector[offset];
            if (first_byte == 0x00)
            {
                return 0;
            }
            if (first_byte == 0xE5)
            {
                continue;
            }

            uint8_t attributes = sector[offset + 0x0B];
            if ((attributes & FAT16_ATTR_LONG_NAME) == FAT16_ATTR_LONG_NAME)
            {
                continue;
            }
            if (attributes & FAT16_ATTR_VOLUME_ID)
            {
                continue;
            }
            if (attributes & FAT16_ATTR_DIRECTORY)
            {
                continue;
            }

            if (fat16_memcmp(&sector[offset], target_name, FAT16_MAX_FILE_NAME) == 0)
            {
                *out_cluster = fat16_read_le16(&sector[offset + 0x1A]);
                *out_size = fat16_read_le32(&sector[offset + 0x1C]);
                return 1;
            }
        }
    }

    return 0;
}

int fat16_mount(fat16_fs_t *fs, uint32_t volume_lba)
{
    if (fs == NULL)
    {
        return -1;
    }

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

    fat16_memcpy(&fat16_mounted_fs, fs, sizeof(fat16_mounted_fs));
    fat16_current_fs = &fat16_mounted_fs;

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

int fat16_open(fat16_fs_t *fs, const char *path)
{
    const fat16_fs_t *use_fs = (fs != NULL) ? fs : fat16_current_fs;
    uint8_t normalized[FAT16_MAX_FILE_NAME];
    uint16_t start_cluster;
    uint32_t file_size;

    if (use_fs == NULL || path == NULL)
    {
        return -1;
    }

    if (fat16_normalize_short_name(path, normalized) < 0)
    {
        return -1;
    }

    int found = fat16_find_root_entry(use_fs, normalized, &start_cluster, &file_size);
    if (found <= 0)
    {
        return -1;
    }

    if (file_size > 0 && start_cluster < FAT16_CLUSTER_MIN)
    {
        return -1;
    }

    for (int index = 0; index < FAT16_MAX_OPEN_FILES; ++index)
    {
        if (!fat16_files[index].active)
        {
            fat16_files[index].fs = use_fs;
            fat16_files[index].start_cluster = start_cluster;
            fat16_files[index].current_cluster = start_cluster;
            fat16_files[index].size = file_size;
            fat16_files[index].position = 0;
            fat16_files[index].active = 1;
            return index + FAT16_FD_BASE;
        }
    }

    return -1;
}

int fat16_read(int fd, uint8_t *buffer, uint32_t count)
{
    if (buffer == NULL || count == 0)
    {
        return -1;
    }

    int index = fd - FAT16_FD_BASE;
    if (index < 0 || index >= FAT16_MAX_OPEN_FILES)
    {
        return -1;
    }

    fat16_file_t *file = &fat16_files[index];
    if (!file->active || file->fs == NULL)
    {
        return -1;
    }

    if (file->position >= file->size)
    {
        return 0;
    }

    uint32_t remaining = file->size - file->position;
    if (remaining > count)
    {
        remaining = count;
    }

    uint32_t total_read = 0;
    uint8_t sector[512];

    while (remaining > 0)
    {
        if (file->current_cluster < FAT16_CLUSTER_MIN)
        {
            break;
        }

        uint32_t cluster_size = (uint32_t)file->fs->bytes_per_sector * file->fs->sectors_per_cluster;
        uint32_t cluster_offset = file->position % cluster_size;
        uint32_t sector_in_cluster = cluster_offset / file->fs->bytes_per_sector;
        uint32_t sector_offset = cluster_offset % file->fs->bytes_per_sector;
        uint32_t lba = fat16_cluster_to_lba(file->fs, file->current_cluster) + sector_in_cluster;

        if (block_read(lba, sector, 1) < 0)
        {
            return -1;
        }

        uint32_t chunk = file->fs->bytes_per_sector - sector_offset;
        if (chunk > remaining)
        {
            chunk = remaining;
        }

        fat16_memcpy(buffer, sector + sector_offset, chunk);
        buffer += chunk;
        remaining -= chunk;
        total_read += chunk;
        file->position += chunk;

        if (file->position < file->size && sector_in_cluster == (uint32_t)file->fs->sectors_per_cluster - 1u && (sector_offset + chunk) == file->fs->bytes_per_sector)
        {
            uint16_t next_cluster = fat16_read_fat_entry(file->fs, file->current_cluster);
            if (next_cluster < FAT16_CLUSTER_MIN || fat16_is_eoc(next_cluster))
            {
                break;
            }
            file->current_cluster = next_cluster;
        }
    }

    return (int)total_read;
}

int fat16_close(int fd)
{
    int index = fd - FAT16_FD_BASE;
    if (index < 0 || index >= FAT16_MAX_OPEN_FILES)
    {
        return -1;
    }

    if (!fat16_files[index].active)
    {
        return -1;
    }

    memset(&fat16_files[index], 0, sizeof(fat16_file_t));
    return 0;
}
