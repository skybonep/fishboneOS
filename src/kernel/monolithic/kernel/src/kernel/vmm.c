#include <kernel/vmm.h>
#include <kernel/pmm.h>
#include <kernel/paging.h>
#include <kernel/memory_map.h>
#include <kernel/cpu.h>
#include <string.h>

// Assembly helper to invalidate a TLB entry (defined in paging.s)
extern void invalidate_tlb_entry(uint32_t vaddr);

static uint32_t current_pdt_phys = 0;
static uint32_t kernel_pdt_phys = 0;

/**
 * vmm_init:
 * Stores the physical address of the initial Page Directory.
 */
void vmm_init(uint32_t pdt_phys_addr)
{
    current_pdt_phys = pdt_phys_addr;
    kernel_pdt_phys = pdt_phys_addr;
}

uint32_t vmm_get_kernel_pdt_phys(void)
{
    return kernel_pdt_phys;
}

uint32_t vmm_clone_kernel_mappings(void)
{
    uint32_t new_pdt_phys = (uint32_t)pmm_alloc_frame();
    if (new_pdt_phys == 0)
    {
        return 0;
    }

    uint32_t *kernel_pdt = (uint32_t *)VMM_PDT_VIRTUAL_ADDR;
    uint32_t *kernel_pt = (uint32_t *)((uint32_t)0xFFC00000 + (KERNEL_VIRT_BASE >> 22) * PAGE_SIZE); // Kernel PT entry base

    // Map new page directory into temporary window (0xC03FF000)
    kernel_pt[1023] = new_pdt_phys | PAGE_PRESENT | PAGE_WRITE;
    invalidate_tlb_entry(VMM_TEMP_PAGE);

    memset((void *)VMM_TEMP_PAGE, 0, PAGE_SIZE);

    uint32_t kernel_base_pde = KERNEL_VIRT_BASE >> 22;
    for (uint32_t i = 0; i < kernel_base_pde; ++i)
    {
        ((uint32_t *)VMM_TEMP_PAGE)[i] = kernel_pdt[i];
    }
    for (uint32_t i = kernel_base_pde; i < 1023; ++i)
    {
        ((uint32_t *)VMM_TEMP_PAGE)[i] = kernel_pdt[i];
    }

    // Set recursive mapping inside new PDT for itself
    ((uint32_t *)VMM_TEMP_PAGE)[1023] = new_pdt_phys | PAGE_PRESENT | PAGE_WRITE;

    // Unmap temporary window
    kernel_pt[1023] = 0;
    invalidate_tlb_entry(VMM_TEMP_PAGE);

    return new_pdt_phys;
}

void vmm_map_page_for_pdt(uint32_t pdt_phys, uint32_t vaddr, uint32_t paddr, uint32_t flags)
{
    uint32_t old_cr3 = read_cr3();
    load_cr3(pdt_phys);
    vmm_map_page(vaddr, paddr, flags);
    load_cr3(old_cr3);
}

void vmm_unmap_page_for_pdt(uint32_t pdt_phys, uint32_t vaddr)
{
    uint32_t old_cr3 = read_cr3();
    load_cr3(pdt_phys);
    vmm_unmap_page(vaddr);
    load_cr3(old_cr3);
}

void vmm_free_page_directory(uint32_t pdt_phys)
{
    if (pdt_phys == 0 || pdt_phys == kernel_pdt_phys)
        return;

    uint32_t old_cr3 = read_cr3();
    load_cr3(pdt_phys);

    uint32_t *pdt = (uint32_t *)VMM_PDT_VIRTUAL_ADDR;
    uint32_t kernel_base_pde = KERNEL_VIRT_BASE >> 22;

    for (uint32_t pde_idx = 0; pde_idx < kernel_base_pde; ++pde_idx)
    {
        if (!(pdt[pde_idx] & PAGE_PRESENT))
            continue;

        uint32_t pt_phys = pdt[pde_idx] & 0xFFFFF000;
        if (pt_phys != 0)
        {
            pdt[pde_idx] = 0;
            pmm_free_frame((void *)(uintptr_t)pt_phys);
        }
    }

    // Remove recursive mapping before we switch away
    pdt[1023] = 0;
    invalidate_tlb_entry(VMM_PDT_VIRTUAL_ADDR);

    load_cr3(old_cr3);
    pmm_free_frame((void *)(uintptr_t)pdt_phys);
}

void vmm_map_kernel_page(uint32_t vaddr, uint32_t paddr)
{
    vmm_map_page(vaddr, paddr, PAGE_PRESENT | PAGE_WRITE);
}

void vmm_map_user_page(uint32_t vaddr, uint32_t paddr)
{
    vmm_map_page(vaddr, paddr, PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
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
        uint32_t *kernel_pt = (uint32_t *)((uint32_t)0xFFC00000 + (KERNEL_VIRT_BASE >> 22) * PAGE_SIZE); // Kernel PT (PDE for 0xC0000000)

        // Temporarily map the new page table at 0xC03FF000 via the last slot in kernel PT
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