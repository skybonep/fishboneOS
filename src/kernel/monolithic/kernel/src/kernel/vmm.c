#include <kernel/vmm.h>
#include <kernel/pmm.h>
#include <kernel/paging.h>
#include <string.h>

// Assembly helper to invalidate a TLB entry (defined in paging.s)
extern void invalidate_tlb_entry(uint32_t vaddr);

static uint32_t current_pdt_phys = 0;

/**
 * vmm_init:
 * Stores the physical address of the initial Page Directory.
 */
void vmm_init(uint32_t pdt_phys_addr)
{
    current_pdt_phys = pdt_phys_addr;
}

/**
 * vmm_map_page:
 * Maps a virtual address to a physical frame in the paging structures [6].
 */
void vmm_map_page(uint32_t vaddr, uint32_t paddr, uint32_t flags)
{
    // 1. Extract indices using the 10-10-12 bit split [7]
    uint32_t pde_idx = vaddr >> 22;           // Top 10 bits: PDT index
    uint32_t pte_idx = (vaddr >> 12) & 0x3FF; // Mid 10 bits: PT index

    // 2. Access the PDT via the recursive mapping address [3, 8]
    uint32_t *pdt = (uint32_t *)VMM_PDT_VIRTUAL_ADDR;

    // 3. Check if the required Page Table exists [7]
    if (!(pdt[pde_idx] & PAGE_PRESENT))
    {
        // BREAK CIRCULAR DEPENDENCY: Allocate a new frame for the PT [1]
        uint32_t new_pt_paddr = (uint32_t)pmm_alloc_frame();
        if (new_pt_paddr == 0)
            return; // Should return NULL/error [9]

        /*
         * To clear the table, map it to our virtual "Temporary Window" (0xC03FF000).
         * This window is the 1023rd entry of the first kernel PT (PDE 768) [2, 4].
         */
        uint32_t *kernel_pt = (uint32_t *)0xFFF00000; // Recusive address for PT 768

        // Correctly assign the physical address to the 1023rd slot [2]
        kernel_pt[1023] = new_pt_paddr | PAGE_PRESENT | PAGE_WRITE;

        // Refresh TLB for the temporary window [5]
        invalidate_tlb_entry(VMM_TEMP_PAGE);

        // Clear the new PT frame memory using the temporary virtual address [4]
        memset((void *)VMM_TEMP_PAGE, 0, PAGE_SIZE);

        // Officially link the new PT into the Page Directory [7, 10]
        pdt[pde_idx] = new_pt_paddr | flags | PAGE_PRESENT;

        // Clean up: Unmap the temporary window
        kernel_pt[1023] = 0;
        invalidate_tlb_entry(VMM_TEMP_PAGE);
    }

    // 4. Access the target Page Table using recursive mapping math
    // 0xFFC00000 is the start of all PTs in a recursive setup [3]
    uint32_t *pt = (uint32_t *)((0xFFC00000) + (pde_idx * PAGE_SIZE));

    // 5. Map the virtual page to the physical frame [7, 10]
    pt[pte_idx] = (paddr & 0xFFFFF000) | flags | PAGE_PRESENT;

    // 6. Invalidate TLB so the CPU sees the new translation [5]
    invalidate_tlb_entry(vaddr);
}

/**
 * vmm_unmap_page:
 * Clears a mapping and invalidates the TLB.
 */
void vmm_unmap_page(uint32_t vaddr)
{
    uint32_t pde_idx = vaddr >> 22;
    uint32_t pte_idx = (vaddr >> 12) & 0x3FF;

    uint32_t *pt = (uint32_t *)((0xFFC00000) + (pde_idx * PAGE_SIZE));
    pt[pte_idx] = 0; // Mark as Not Present

    invalidate_tlb_entry(vaddr);
}

/**
 * vmm_get_phys_addr:
 * Translates a virtual address back to physical RAM.
 */
uint32_t vmm_get_phys_addr(uint32_t vaddr)
{
    uint32_t pde_idx = vaddr >> 22;
    uint32_t pte_idx = (vaddr >> 12) & 0x3FF;
    uint32_t offset = vaddr & 0xFFF;

    uint32_t *pdt = (uint32_t *)VMM_PDT_VIRTUAL_ADDR;

    if (!(pdt[pde_idx] & PAGE_PRESENT))
        return 0;

    uint32_t *pt = (uint32_t *)((0xFFC00000) + (pde_idx * PAGE_SIZE));
    if (!(pt[pte_idx] & PAGE_PRESENT))
        return 0;

    return (pt[pte_idx] & 0xFFFFF000) + offset;
}