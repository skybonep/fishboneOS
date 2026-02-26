#ifndef KERNEL_VMM_H
#define KERNEL_VMM_H

#include <stdint.h>
#include <stdbool.h>

/* Page frame size: 4096 bytes [3] */
#define PAGE_SIZE 4096

/* The virtual address where the Page Directory is mapped for editing. */
/* This resides in the higher-half of the address space [4]. */
#define VMM_PDT_VIRTUAL_ADDR 0xFFFFF000 

/* Temporary mapping window used to break the "circular dependency" [5]. */
/* This is the last entry of the kernel's first Page Table [6]. */
#define VMM_TEMP_PAGE 0xC03FF000

/**
 * vmm_init:
 * Initializes the VMM state. This typically involves ensuring the 
 * Page Directory is mapped into virtual memory so the kernel can edit it.
 */
void vmm_init(uint32_t pdt_phys_addr);

/**
 * vmm_map_page:
 * Maps a virtual address (vaddr) to a physical address (paddr) with flags.
 * This function handles the allocation of new Page Tables if needed [3, 7].
 */
void vmm_map_page(uint32_t vaddr, uint32_t paddr, uint32_t flags);

/**
 * vmm_unmap_page:
 * Removes the mapping for a virtual address and invalidates its TLB entry [8].
 */
void vmm_unmap_page(uint32_t vaddr);

/**
 * vmm_get_phys_addr:
 * Translates a virtual address to its physical address using the 
 * current Page Directory and Page Tables [9, 10].
 * Returns 0 if no mapping exists.
 */
uint32_t vmm_get_phys_addr(uint32_t vaddr);

#endif