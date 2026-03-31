#include <kernel/log.h>
#include <kernel/memory_map.h>

void log_memory_layout(void)
{
    printk(LOG_INFO, "--- Memory Layout ---");
    printk(LOG_INFO, "Physical reserved:   0x%08x - 0x%08x",
           BIOS_BOOT_RESERVED_START, BIOS_BOOT_RESERVED_END);
    printk(LOG_INFO, "Physical identity:   0x%08x - 0x%08x",
           IDENTITY_MAP_START, IDENTITY_MAP_END);
    printk(LOG_INFO, "Kernel physical:     0x%08x - 0x%08x",
           PHYS_KERNEL_BASE, PHYS_KERNEL_END);
    printk(LOG_INFO, "Kernel virtual base: 0x%08x", KERNEL_VIRT_BASE);
    printk(LOG_INFO, "Kernel stack region: 0x%08x - 0x%08x",
           KERNEL_STACK_REGION_START, KERNEL_STACK_REGION_END);
    printk(LOG_INFO, "Kernel heap region:  0x%08x - 0x%08x",
           KERNEL_HEAP_START, KERNEL_HEAP_END);
    printk(LOG_INFO, "Recursive VMM page:  0x%08x", VMM_PDT_VIRTUAL_ADDR);
    printk(LOG_INFO, "Temporary VMM page:  0x%08x", VMM_TEMP_PAGE);
    printk(LOG_INFO, "User-space range:    0x%08x - 0x%08x",
           USER_SPACE_START, USER_SPACE_END);
    printk(LOG_INFO, "MMIO reserved start: 0x%08x", MMIO_RESERVED_START);
    printk(LOG_INFO, "------------------------------");
}
