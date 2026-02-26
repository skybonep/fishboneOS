#ifndef KERNEL_PMM_H
#define KERNEL_PMM_H

#include <stdint.h>
#include <kernel/multiboot.h>

/* 
 * The physical reality of x86: memory is divided into 4 KiB (4096 byte) 
 * page frames. 
 */
#define PAGE_SIZE 4096

/* 
 * BITMAP_SIZE: Supports up to 128 MB of physical RAM.
 * (128 MB / 4 KB per frame) / 8 bits per byte = 4096 bytes for the bitmap.
 */
#define BITMAP_SIZE 4096

/**
 * pmm_init:
 * Initializes the bitmap by parsing the Multiboot memory map.
 * It marks available RAM as free and ensures the kernel's physical 
 * memory is reserved.
 * 
 * @param mbinfo Pointer to the Multiboot information structure.
 */
void pmm_init(multiboot_info_t *mbinfo);

/**
 * pmm_alloc_frame:
 * Searches the bitmap for the first available physical page frame.
 * 
 * @return The physical address of the allocated frame, or NULL if out of memory.
 */
void *pmm_alloc_frame(void);

/**
 * pmm_free_frame:
 * Marks a physical page frame as available in the bitmap.
 * 
 * @param paddr The physical address of the frame to free.
 */
void pmm_free_frame(void *paddr);



#endif /* KERNEL_PMM_H */