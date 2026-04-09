#ifndef DRIVERS_ATA_H
#define DRIVERS_ATA_H

#include <stdint.h>

/**
 * Initialize the primary ATA controller and detect the primary master drive.
 * @return 0 on success, -1 on failure.
 */
int ata_init(void);

/**
 * Check whether a primary ATA master device is present.
 * @return 1 if present, 0 if not.
 */
int ata_is_present(void);

/**
 * Read a single 512-byte sector from the primary ATA drive.
 * @param lba Logical block address to read.
 * @param buffer Destination buffer, must be at least 512 bytes.
 * @return 0 on success, -1 on failure.
 */
int ata_read_sector(uint32_t lba, void *buffer);

/**
 * Write a single 512-byte sector to the primary ATA drive.
 * @param lba Logical block address to write.
 * @param buffer Source buffer, must be at least 512 bytes.
 * @return 0 on success, -1 on failure.
 */
int ata_write_sector(uint32_t lba, const void *buffer);

/**
 * Read multiple 512-byte sectors from the primary ATA drive.
 * @param lba Starting logical block address.
 * @param buffer Destination buffer, must be at least 512 * count bytes.
 * @param count Number of sectors to read.
 * @return 0 on success, -1 on failure.
 */
int ata_read_sectors(uint32_t lba, void *buffer, uint32_t count);

/**
 * Write multiple 512-byte sectors to the primary ATA drive.
 * @param lba Starting logical block address.
 * @param buffer Source buffer, must be at least 512 * count bytes.
 * @param count Number of sectors to write.
 * @return 0 on success, -1 on failure.
 */
int ata_write_sectors(uint32_t lba, const void *buffer, uint32_t count);

#endif /* DRIVERS_ATA_H */
