#include <stdint.h>
#include <stddef.h>
#include <kernel/pmm.h>
//#include <kernel/paging.h> // Assuming virtual memory mapping functions exist

#define HEAP_START 0xC0400000 // Higher-half start, above the kernel
#define PAGE_SIZE 4096

typedef struct header {
    size_t size;           // Size of the data block (not including header)
    int is_free;           // 1 if free, 0 if allocated
    struct header *next;   // Next chunk in the doubly-linked list [1]
    struct header *prev;   // Previous chunk for coalescing [1]
} header_t;

void* kmalloc(size_t size);
void kfree(void *ptr);

// static header_t *free_list_start = NULL;
// static uint32_t heap_end_vaddr = HEAP_START;