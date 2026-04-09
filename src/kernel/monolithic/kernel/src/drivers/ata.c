#include <stddef.h>
#include <stdint.h>

#include <kernel/io.h>
#include <kernel/log.h>
#include <drivers/ata.h>

#define ATA_PRIMARY_IO_BASE 0x1F0
#define ATA_PRIMARY_CTRL_BASE 0x3F6

#define ATA_REG_DATA 0x00
#define ATA_REG_ERROR 0x01
#define ATA_REG_FEATURES 0x01
#define ATA_REG_SECTOR_COUNT 0x02
#define ATA_REG_LBA_LOW 0x03
#define ATA_REG_LBA_MID 0x04
#define ATA_REG_LBA_HIGH 0x05
#define ATA_REG_DRIVE_HEAD 0x06
#define ATA_REG_STATUS 0x07
#define ATA_REG_COMMAND 0x07

#define ATA_CMD_READ_SECTORS 0x20
#define ATA_CMD_WRITE_SECTORS 0x30

#define ATA_SR_ERR 0x01
#define ATA_SR_DRQ 0x08
#define ATA_SR_DF 0x20
#define ATA_SR_BSY 0x80

static void ata_io_wait(void)
{
    /* Give the drive time to process the command. */
    (void)inb(ATA_PRIMARY_CTRL_BASE);
    (void)inb(ATA_PRIMARY_CTRL_BASE);
    (void)inb(ATA_PRIMARY_CTRL_BASE);
    (void)inb(ATA_PRIMARY_CTRL_BASE);
}

static int ata_wait_not_busy(void)
{
    for (uint32_t i = 0; i < 1000000; ++i)
    {
        uint8_t status = inb(ATA_PRIMARY_IO_BASE + ATA_REG_STATUS);
        if (!(status & ATA_SR_BSY))
            return 0;
    }
    return -1;
}

static int ata_wait_drq(void)
{
    for (uint32_t i = 0; i < 1000000; ++i)
    {
        uint8_t status = inb(ATA_PRIMARY_IO_BASE + ATA_REG_STATUS);
        if (status & ATA_SR_ERR)
            return -1;
        if (!(status & ATA_SR_BSY) && (status & ATA_SR_DRQ))
            return 0;
    }
    return -1;
}

static int ata_set_lba(uint32_t lba, uint8_t count)
{
    if (count == 0)
        return -1;

    outb(ATA_PRIMARY_IO_BASE + ATA_REG_SECTOR_COUNT, count);
    outb(ATA_PRIMARY_IO_BASE + ATA_REG_LBA_LOW, (uint8_t)(lba & 0xFF));
    outb(ATA_PRIMARY_IO_BASE + ATA_REG_LBA_MID, (uint8_t)((lba >> 8) & 0xFF));
    outb(ATA_PRIMARY_IO_BASE + ATA_REG_LBA_HIGH, (uint8_t)((lba >> 16) & 0xFF));
    outb(ATA_PRIMARY_IO_BASE + ATA_REG_DRIVE_HEAD,
         (uint8_t)(0xE0 | ((lba >> 24) & 0x0F)));
    return 0;
}

static void ata_read_data(uint16_t *buffer)
{
    for (int i = 0; i < 256; ++i)
    {
        buffer[i] = inw(ATA_PRIMARY_IO_BASE + ATA_REG_DATA);
    }
}

static void ata_write_data(const uint16_t *buffer)
{
    for (int i = 0; i < 256; ++i)
    {
        outw(ATA_PRIMARY_IO_BASE + ATA_REG_DATA, buffer[i]);
    }
}

static int ata_read_sector_internal(uint32_t lba, void *buffer)
{
    if (buffer == NULL)
        return -1;

    if (ata_wait_not_busy() < 0)
        return -1;

    if (ata_set_lba(lba, 1) < 0)
        return -1;

    outb(ATA_PRIMARY_IO_BASE + ATA_REG_COMMAND, ATA_CMD_READ_SECTORS);
    if (ata_wait_drq() < 0)
        return -1;

    ata_read_data((uint16_t *)buffer);
    ata_io_wait();
    return 0;
}

static int ata_write_sector_internal(uint32_t lba, const void *buffer)
{
    if (buffer == NULL)
        return -1;

    if (ata_wait_not_busy() < 0)
        return -1;

    if (ata_set_lba(lba, 1) < 0)
        return -1;

    outb(ATA_PRIMARY_IO_BASE + ATA_REG_COMMAND, ATA_CMD_WRITE_SECTORS);
    if (ata_wait_drq() < 0)
        return -1;

    ata_write_data((const uint16_t *)buffer);
    ata_io_wait();
    return ata_wait_not_busy();
}

int ata_init(void)
{
    if (ata_wait_not_busy() < 0)
    {
        printk(LOG_ERROR, "ATA: primary master not responding");
        return -1;
    }

    /* Select the primary master drive. */
    outb(ATA_PRIMARY_IO_BASE + ATA_REG_DRIVE_HEAD, 0xA0);
    ata_io_wait();

    uint8_t status = inb(ATA_PRIMARY_IO_BASE + ATA_REG_STATUS);
    if (status == 0xFF || status == 0x00)
    {
        printk(LOG_ERROR, "ATA: no drive detected on primary master");
        return -1;
    }

    printk(LOG_INFO, "ATA: primary master detected");
    return 0;
}

int ata_is_present(void)
{
    return ata_init() == 0;
}

int ata_read_sector(uint32_t lba, void *buffer)
{
    return ata_read_sector_internal(lba, buffer);
}

int ata_write_sector(uint32_t lba, const void *buffer)
{
    return ata_write_sector_internal(lba, buffer);
}

int ata_read_sectors(uint32_t lba, void *buffer, uint32_t count)
{
    if (buffer == NULL || count == 0)
        return -1;

    for (uint32_t i = 0; i < count; ++i)
    {
        if (ata_read_sector_internal(lba + i, (uint8_t *)buffer + (i * 512)) < 0)
            return -1;
    }
    return 0;
}

int ata_write_sectors(uint32_t lba, const void *buffer, uint32_t count)
{
    if (buffer == NULL || count == 0)
        return -1;

    for (uint32_t i = 0; i < count; ++i)
    {
        if (ata_write_sector_internal(lba + i, (const uint8_t *)buffer + (i * 512)) < 0)
            return -1;
    }
    return 0;
}
