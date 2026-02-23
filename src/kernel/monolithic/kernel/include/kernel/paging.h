#ifndef KERNEL_PAGING_H
#define KERNEL_PAGING_H

#include <stdint.h>

#define PAGE_SIZE 4096
#define ENTRIES_PER_TABLE 1024

// Paging entry flags
#define PAGE_PRESENT  0x1
#define PAGE_WRITE    0x2
#define PAGE_USER     0x4

// A Page Directory Entry (PDE) or Page Table Entry (PTE)
typedef uint32_t page_entry_t;

/**
 * paging_init:
 * Sets up identity paging for the first 4MB and maps the kernel 
 * to the higher half (0xC0000000).
 */
void paging_init(void);

#endif