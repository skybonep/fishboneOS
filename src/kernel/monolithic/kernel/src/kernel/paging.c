#include <kernel/paging.h>
#include <kernel/pmm.h>
#include <string.h>

// Assembly helpers defined in paging_asm.s
extern void load_page_directory(uint32_t* directory);
extern void enable_paging(void);

void paging_init() {
    // 1. Allocate physical frames for PDT and the first PT
    uint32_t* pdt = (uint32_t*)pmm_alloc_frame();
    uint32_t* pt_low = (uint32_t*)pmm_alloc_frame();

    // 2. Clear PDT (mark all entries as Not Present)
    for (int i = 0; i < ENTRIES_PER_TABLE; i++) {
        pdt[i] = 0x00000000;
    }

    // 3. Identity Map the first 4MB (0x0 to 0x3FFFFF)
    // This is the "Safety Bridge" to prevent crashing during the enable step.
    for (int i = 0; i < ENTRIES_PER_TABLE; i++) {
        pt_low[i] = (i * PAGE_SIZE) | PAGE_PRESENT | PAGE_WRITE;
    }

    // 4. Link the Page Table into the Page Directory
    // Index 0 handles virtual addresses 0x0 - 0x3FFFFF
    pdt[0] = (uint32_t)pt_low | PAGE_PRESENT | PAGE_WRITE;

    // 5. Higher-Half Mapping (Preparation for Step 4)
    // Map 0xC0000000 (Index 768) to the same physical PT for now.
    pdt[768] = (uint32_t)pt_low | PAGE_PRESENT | PAGE_WRITE;

    // 6. Enable the hardware
    load_page_directory(pdt);
    enable_paging();
}