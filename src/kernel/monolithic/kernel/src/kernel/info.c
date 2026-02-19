#include <kernel/cpu.h>
#include <kernel/log.h>
#include <kernel/multiboot.h>

#define MULTIBOOT_BOOTLOADER_MAGIC 0x2BADB002

void log_system_info(void)
{
    unsigned int cr0 = read_cr0();
    unsigned int ebx = read_ebx();

    printk(LOG_INFO, "");
    printk(LOG_INFO, "--- fishboneOS System Info ---");

    /* Check Bit 0 of CR0 for Protected Mode */
    if (cr0 & CR0_PE)
    {
        printk(LOG_INFO, "Processor Mode: 32-bit Protected Mode");
    }
    else
    {
        printk(LOG_INFO, "Processor Mode: 16-bit Real Mode");
    }

    /* Check Bit 31 of CR0 for Paging status */
    if (cr0 & CR0_PG)
    {
        printk(LOG_INFO, "Paging: ENABLED");
    }
    else
    {
        printk(LOG_INFO, "Paging: DISABLED");
    }

    printk(LOG_INFO, "------------------------------");
}

/**
 * multiboot_info:
 * Iterates through the multiboot_info_t struct and logs available
 * information using the printk utility based on the flags set by GRUB.
 */
void multiboot_info(unsigned int multiboot_magic, multiboot_info_t *mbinfo)
{
    printk(LOG_INFO, "");
    printk(LOG_INFO, "--- Multiboot Info ---");
    printk(LOG_INFO, "Multiboot magic number: 0x%08x", multiboot_magic);

    if (multiboot_magic != MULTIBOOT_BOOTLOADER_MAGIC) {
        printk(LOG_WARNING, "Invalid magic number: 0x%08x. Expected 0x2BADB002.", multiboot_magic);
        printk(LOG_INFO, "------------------------------");
        return;
	}

    printk(LOG_INFO, "Flags: 0x%08x", mbinfo->flags);

    // Basic Memory Information (Flag bit 0)
    if (mbinfo->flags & (1 << 0))
    {
        printk(LOG_INFO, "Lower Memory: %d KB", mbinfo->mem_lower);
        printk(LOG_INFO, "Upper Memory: %d KB", mbinfo->mem_upper);
    }

    // Boot Device (Flag bit 1)
    if (mbinfo->flags & (1 << 1))
    {
        printk(LOG_INFO, "Boot Device: 0x%08x", mbinfo->boot_device);
    }

    // Kernel Command Line (Flag bit 2)
    if (mbinfo->flags & (1 << 2))
    {
        // Cast cmdline to char* to treat it as a string
        printk(LOG_INFO, "Command Line: %s", (char *)mbinfo->cmdline);
    }

    // Modules (Flag bit 3)
    if (mbinfo->flags & (1 << 3))
    {
        printk(LOG_INFO, "Modules Count: %d | Address: 0x%08x",
               mbinfo->mods_count, mbinfo->mods_addr);
    }

    // Memory Map (Flag bit 6)
    // Critical for your next step: planning the physical memory map [3].
    if (mbinfo->flags & (1 << 6))
    {
        printk(LOG_INFO, "MMap Length: 0x%x | Address: 0x%08x",
               mbinfo->mmap_length, mbinfo->mmap_addr);
    }

	// Iterate through the memory map entries
	if (mbinfo->flags & (1 << 6))
	{
		multiboot_memory_map_t *mmap = (multiboot_memory_map_t *)mbinfo->mmap_addr;

		while ((uint32_t)mmap < mbinfo->mmap_addr + mbinfo->mmap_length)
		{
			// Log each memory segment to your Bochs com1.out [8]
			printk(LOG_INFO, "Memory Area: 0x%08x | Length: 0x%08x | Type: %d",
				   (uint32_t)mmap->addr, (uint32_t)mmap->len, mmap->type);

			mmap = (multiboot_memory_map_t *)((uint32_t)mmap + mmap->size + sizeof(mmap->size));
		}
	} else {
        printk(LOG_ERROR, "Invalid Multiboot memory map!");
    }

    printk(LOG_INFO, "------------------------------");
}