#ifndef DRIVERS_BLOCK_H
#define DRIVERS_BLOCK_H

#include <stdint.h>

/**
 * Initialize the default block device subsystem.
 * @return 0 on success, -1 on failure.
 */
int block_init(void);

/**
 * Read one or more 512-byte sectors from the block device.
 * @param lba Starting logical block address.
 * @param buffer Destination buffer, must be at least 512 * count bytes.
 * @param count Number of sectors to read.
 * @return 0 on success, -1 on failure.
 */
int block_read(uint32_t lba, uint8_t *buffer, uint32_t count);

/**
 * Write one or more 512-byte sectors to the block device.
 * @param lba Starting logical block address.
 * @param buffer Source buffer, must be at least 512 * count bytes.
 * @param count Number of sectors to write.
 * @return 0 on success, -1 on failure.
 */
int block_write(uint32_t lba, const uint8_t *buffer, uint32_t count);

#endif /* DRIVERS_BLOCK_H */
