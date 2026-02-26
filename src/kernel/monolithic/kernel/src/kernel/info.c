#include <kernel/cpu.h>
#include <kernel/log.h>
#include <kernel/multiboot.h>

#define MULTIBOOT_BOOTLOADER_MAGIC 0x2BADB002

/* CR0 Bitmasks */
#define CR0_PE 0x00000001 // Protection Enable
#define CR0_MP 0x00000002 // Monitor Coprocessor
#define CR0_EM 0x00000004 // Emulation
#define CR0_TS 0x00000008 // Task Switched
#define CR0_ET 0x00000010 // Extension Type
#define CR0_NE 0x00000020 // Numeric Error
#define CR0_WP 0x00010000 // Write Protect
#define CR0_AM 0x00040000 // Alignment Mask
#define CR0_NW 0x20000000 // Not Write-through
#define CR0_CD 0x40000000 // Cache Disable
#define CR0_PG 0x80000000 // Paging [2]

/* CR3 Bits */
#define CR3_PWT 0x00000008 // Page-level Writethrough
#define CR3_PCD 0x00000010 // Page-level Cache Disable

/* CR4 Bits */
#define CR4_VME  0x00000001 // Virtual-8086 Mode Extensions
#define CR4_PVI  0x00000002 // Protected-Mode Virtual Interrupts
#define CR4_TSD  0x00000004 // Time Stamp Disable
#define CR4_DE   0x00000008 // Debugging Extensions
#define CR4_PSE  0x00000010 // Page Size Extensions (Source [1])
#define CR4_PAE  0x00000020 // Physical Address Extension
#define CR4_MCE  0x00000040 // Machine-Check Enable
#define CR4_PGE  0x00000080 // Page Global Enable

/**
 * log_cr0_details:
 * Performs a deep-dive inspection of the CR0 register and logs
 * the status of every hardware-level feature toggled.
 */
void log_cr0_details(void)
{
    unsigned int cr0 = read_cr0();

    printk(LOG_INFO, "--- CR0 Register ---");
    printk(LOG_INFO, "Raw CR0 Value: 0x%08x", cr0);

    // Memory and Mode Control [2]
    printk(LOG_INFO, "  [PE] Protected Mode: %s", (cr0 & CR0_PE) ? "ON" : "OFF (Real Mode)");
    printk(LOG_INFO, "  [PG] Paging Status:  %s", (cr0 & CR0_PG) ? "ENABLED" : "DISABLED");
    printk(LOG_INFO, "  [WP] Write Protect:  %s", (cr0 & CR0_WP) ? "ON (Ring 0 cannot write to RO pages)" : "OFF");

    // Cache Control (Relates to Chapter 7 concepts) [3]
    printk(LOG_INFO, "  [CD] Cache Disable:  %s", (cr0 & CR0_CD) ? "YES (No caching)" : "NO (Caching active)");
    printk(LOG_INFO, "  [NW] Write-through:  %s", (cr0 & CR0_NW) ? "DISABLED" : "ENABLED");

    // Floating Point & Multitasking [4]
    printk(LOG_INFO, "  [TS] Task Switched:  %s", (cr0 & CR0_TS) ? "YES (Lazy FPU saving)" : "NO");
    printk(LOG_INFO, "  [EM] FPU Emulation:  %s", (cr0 & CR0_EM) ? "ON" : "OFF");
    printk(LOG_INFO, "  [NE] Numeric Error:  %s", (cr0 & CR0_NE) ? "Internal" : "External (DOS-style)");

    // Alignment and Legacy
    printk(LOG_INFO, "  [AM] Alignment Mask: %s", (cr0 & CR0_AM) ? "ENABLED" : "DISABLED");
    printk(LOG_INFO, "  [ET] Extension Type: %s", (cr0 & CR0_ET) ? "387 Support" : "287 Support");
    printk(LOG_INFO, "---------------------------------");
}

void log_cr2_details(void)
{
    unsigned int cr2 = read_cr2();
    printk(LOG_INFO, "--- CR2 Register ---");
    printk(LOG_INFO, "Last Page Fault Address: 0x%08x", cr2);
    printk(LOG_INFO, "---------------------------------");
}

void log_cr3_details(void)
{
    unsigned int cr3 = read_cr3();
    printk(LOG_INFO, "--- CR3 Register ---");
    printk(LOG_INFO, "Raw CR3 Value: 0x%08x", cr3);
    
    // Bits 31-12 contain the physical address [5, 7]
    unsigned int pdt_phys = cr3 & 0xFFFFF000;
    printk(LOG_INFO, "  PDT Physical Base: 0x%08x", pdt_phys);

    // Lower bits are for cache configuration [5]
    printk(LOG_INFO, "  [PWT] Page Writethrough: %s", (cr3 & CR3_PWT) ? "Enabled" : "Disabled");
    printk(LOG_INFO, "  [PCD] Page Cache Disable: %s", (cr3 & CR3_PCD) ? "Enabled" : "Disabled");
    printk(LOG_INFO, "---------------------------------");
}

void log_cr4_details(void)
{
    unsigned int cr4 = read_cr4();
    printk(LOG_INFO, "--- CR4 Register ---");
    printk(LOG_INFO, "Raw CR4 Value: 0x%08x", cr4);

    // Page Size Extensions (Critical for Step 3/4) [1]
    printk(LOG_INFO, "  [PSE] Page Size Extensions: %s", (cr4 & CR4_PSE) ? "ENABLED (4MB pages allowed)" : "DISABLED");
    
    // Other Architectural Extensions (Non-source info)
    printk(LOG_INFO, "  [PAE] Physical Address Ext: %s", (cr4 & CR4_PAE) ? "ON" : "OFF");
    printk(LOG_INFO, "  [PGE] Global Page Enable:   %s", (cr4 & CR4_PGE) ? "ON" : "OFF");
    printk(LOG_INFO, "  [VME] Virtual-8086 Ext:     %s", (cr4 & CR4_VME) ? "ON" : "OFF");
    printk(LOG_INFO, "---------------------------------");
}

void log_system_info(void)
{
    log_cr0_details();
    log_cr2_details();
    log_cr3_details();
    log_cr4_details();
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