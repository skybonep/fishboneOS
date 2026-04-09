#include <stdint.h>

#include <drivers/block.h>
#include <drivers/ata.h>

int block_init(void)
{
    return ata_init();
}

int block_read(uint32_t lba, uint8_t *buffer, uint32_t count)
{
    return ata_read_sectors(lba, buffer, count);
}

int block_write(uint32_t lba, const uint8_t *buffer, uint32_t count)
{
    return ata_write_sectors(lba, buffer, count);
}
