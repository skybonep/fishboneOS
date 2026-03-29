#include <kernel/paging.h>
#include <kernel/pmm.h>
#include <kernel/memory_map.h>
#include <string.h>

// Assembly helpers defined in paging.s
extern void load_page_directory(uint32_t *directory);
extern void enable_paging(void);

/**
 * paging_init:
 * Bootstraps the paging system, maps the kernel into the higher-half,
 * and sets up recursive mapping for the VMM.
 */
void paging_init()
{
    // 1. Allocate physical frames for the PDT and the initial Page Table.
    // Paging structures MUST be aligned on 4096-byte boundaries.
    uint32_t pdt_phys = (uint32_t)pmm_alloc_frame();
    uint32_t pt_low_phys = (uint32_t)pmm_alloc_frame();

    // While bootstrapping (before paging is enabled), we can access
    // these physical addresses directly.
    uint32_t *pdt = (uint32_t *)pdt_phys;
    uint32_t *pt_low = (uint32_t *)pt_low_phys;

    // 2. Clear the Page Directory to ensure all entries are 'Not Present'.
    for (int i = 0; i < 1024; i++)
    {
        pdt[i] = 0x00000000;
    }

    // 3. Identity Map the first 4 MB (0x0 to 0x3FFFFF).
    // This prevents the CPU from crashing the moment paging is enabled.
    uint32_t identity_entries = (IDENTITY_MAP_END - IDENTITY_MAP_START + 1) / PAGE_SIZE;
    for (uint32_t i = 0; i < identity_entries; i++)
    {
        pt_low[i] = (i * PAGE_SIZE) | PAGE_PRESENT | PAGE_WRITE;
    }

    // 4. Link the PT into the Page Directory at Index 0.
    // This handles the current physical location of your code.
    pdt[0] = pt_low_phys | PAGE_PRESENT | PAGE_WRITE;

    // 5. Higher-Half Mapping (Index based on KERNEL_VIRT_BASE).
    // Maps virtual KERNEL_VIRT_BASE to the same physical first 4 MB.
    uint32_t kernel_pde_idx = KERNEL_VIRT_BASE >> 22;
    pdt[kernel_pde_idx] = pt_low_phys | PAGE_PRESENT | PAGE_WRITE;

    // 6. THE RECURSIVE MAPPING ENTRY (Index 1023).
    // Map the PDT to itself. This allows the VMM to see the paging structures
    // as data at virtual address 0xFFFFF000.
    pdt[1023] = pdt_phys | PAGE_PRESENT | PAGE_WRITE;

    // 7. Commit to Hardware.
    // The load_page_directory function expects the PHYSICAL address.
    load_page_directory((uint32_t *)pdt_phys);
    enable_paging();

    // Once enabled, the CPU uses the 'Desirable Illusion' of virtual addresses.
}
