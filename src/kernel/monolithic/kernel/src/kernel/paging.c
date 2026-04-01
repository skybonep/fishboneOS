#include <kernel/paging.h>
#include <kernel/pmm.h>
#include <kernel/memory_map.h>
#include <string.h>

// Assembly helpers defined in paging.s
extern void load_page_directory(uint32_t *directory);
extern void enable_paging(void);

// Linker labels exported from linker.ld for the kernel physical image.
extern uint32_t kernel_physical_start;
extern uint32_t kernel_physical_end;

/**
 * paging_init:
 * Bootstraps the paging system, maps the kernel into the higher-half,
 * and sets up recursive mapping for the VMM.
 */
void paging_init()
{
    // 1. Allocate physical frames for the PDT and initial Page Tables.
    uint32_t pdt_phys = (uint32_t)pmm_alloc_frame();
    uint32_t pt_low_phys = (uint32_t)pmm_alloc_frame();
    uint32_t pt_kernel_phys = (uint32_t)pmm_alloc_frame();

    uint32_t *pdt = (uint32_t *)pdt_phys;
    uint32_t *pt_low = (uint32_t *)pt_low_phys;
    uint32_t *pt_kernel = (uint32_t *)pt_kernel_phys;

    // 2. Clear the Page Directory and page tables.
    memset(pdt, 0, PAGE_SIZE);
    memset(pt_low, 0, PAGE_SIZE);
    memset(pt_kernel, 0, PAGE_SIZE);

    // 3. Identity Map the first 4 MB (0x00000000-0x003FFFFF).
    uint32_t identity_entries = (IDENTITY_MAP_END - IDENTITY_MAP_START + 1) / PAGE_SIZE;
    for (uint32_t i = 0; i < identity_entries; i++)
    {
        pt_low[i] = (i * PAGE_SIZE) | PAGE_PRESENT | PAGE_WRITE;
    }
    pdt[0] = pt_low_phys | PAGE_PRESENT | PAGE_WRITE;

    // 4. Map the kernel physical image into the higher-half.
    uint32_t kernel_pde_idx = KERNEL_VIRT_BASE >> 22;
    uint32_t kernel_start_page = (uint32_t)&kernel_physical_start / PAGE_SIZE;
    uint32_t kernel_end_page = (((uint32_t)&kernel_physical_end - 1) / PAGE_SIZE);
    uint32_t kernel_page_count = kernel_end_page - kernel_start_page + 1;

    for (uint32_t i = 0; i < kernel_page_count; i++)
    {
        pt_kernel[i] = ((kernel_start_page + i) * PAGE_SIZE) | PAGE_PRESENT | PAGE_WRITE;
    }
    pdt[kernel_pde_idx] = pt_kernel_phys | PAGE_PRESENT | PAGE_WRITE;

    // 5. Reserve the heap region with a dedicated PDE entry.
    uint32_t heap_pde_idx = KERNEL_HEAP_START >> 22;
    pdt[heap_pde_idx] = 0; // Leave unmapped until the heap is actually used.

    // 6. Map the kernel stack region into the same higher-half PT.
    uint32_t stack_pde_idx = KERNEL_STACK_REGION_START >> 22;
    uint32_t stack_pt_index = (KERNEL_STACK_REGION_START - KERNEL_VIRT_BASE) / PAGE_SIZE;
    uint32_t stack_region_pages = (KERNEL_STACK_REGION_END - KERNEL_STACK_REGION_START + 1) / PAGE_SIZE;
    for (uint32_t i = 0; i < stack_region_pages; i++)
    {
        void *stack_frame = pmm_alloc_frame();
        pt_kernel[stack_pt_index + i] = (uint32_t)stack_frame | PAGE_PRESENT | PAGE_WRITE;
    }
    pdt[stack_pde_idx] = pt_kernel_phys | PAGE_PRESENT | PAGE_WRITE;

    // 7. Recursive mapping entry (Index 1023) for VMM access.
    pdt[1023] = pdt_phys | PAGE_PRESENT | PAGE_WRITE;

    // 8. Install paging.
    load_page_directory((uint32_t *)pdt_phys);
    enable_paging();
}
