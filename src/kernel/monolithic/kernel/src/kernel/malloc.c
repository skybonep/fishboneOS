#include <kernel/malloc.h>
#include <kernel/pmm.h>
#include <kernel/vmm.h>
#include <kernel/paging.h>
#include <string.h>

/* 
 * The Free List: A doubly-linked list of memory chunks [1].
 * Every chunk is prefixed with a header (boundary tag) [1].
 */
static header_t *free_list_start = NULL;
static uint32_t heap_end_vaddr = HEAP_START;

/**
 * expand_heap:
 * Acts as the internal sbrk. Requests physical frames from the PMM
 * and maps them into the higher-half virtual address space.
 */
static header_t* expand_heap(size_t size) {
    size_t total_required = size + sizeof(header_t);
    uint32_t num_pages = (total_required + PAGE_SIZE - 1) / PAGE_SIZE;

    header_t *new_chunk = (header_t*)heap_end_vaddr;

    for (uint32_t i = 0; i < num_pages; i++) {
        void* phys_frame = pmm_alloc_frame();
        if (phys_frame == NULL) return NULL; // Deserves a "paddlin'" [5, 6]

        // Map the physical frame to the virtual heap address [2]
        vmm_map_page(heap_end_vaddr, (uint32_t)phys_frame, PAGE_PRESENT | PAGE_WRITE);
        heap_end_vaddr += PAGE_SIZE;
    }

    new_chunk->size = (num_pages * PAGE_SIZE) - sizeof(header_t);
    new_chunk->is_free = 1;
    new_chunk->next = NULL;
    new_chunk->prev = NULL;

    return new_chunk;
}

/**
 * kmalloc:
 * Searches the free list for a chunk of at least 'size' bytes.
 */
void* kmalloc(size_t size) {
    if (size == 0) return NULL;

    // Align size to 16 bytes for efficiency and hardware requirements
    size = (size + 15) & ~15;

    header_t *curr = free_list_start;

    // First-Fit Search: Traverse the list to find a free chunk
    while (curr != NULL) {
        if (curr->is_free && curr->size >= size) {
            // Split the chunk if there is enough leftover space for a new header
            if (curr->size >= size + sizeof(header_t) + 16) {
                header_t *split = (header_t*)((char*)curr + sizeof(header_t) + size);
                split->size = curr->size - size - sizeof(header_t);
                split->is_free = 1;
                split->next = curr->next;
                split->prev = curr;
                if (curr->next) curr->next->prev = split;
                curr->next = split;
                curr->size = size;
            }
            curr->is_free = 0;
            return (void*)((char*)curr + sizeof(header_t));
        }
        curr = curr->next;
    }

    // If no chunk is found, expand the heap
    header_t *new_block = expand_heap(size);
    if (!new_block) return NULL;

    // Link the new block into the list
    if (!free_list_start) {
        free_list_start = new_block;
    } else {
        header_t *last = free_list_start;
        while (last->next) last = last->next;
        last->next = new_block;
        new_block->prev = last;
    }

    return kmalloc(size); // Retry allocation with the new space
}

/**
 * kfree:
 * Marks a chunk as free and performs coalescing to prevent fragmentation.
 */
void kfree(void *ptr) {
    if (!ptr) return;

    // Access the boundary tag located immediately before the data [1]
    header_t *header = (header_t*)((char*)ptr - sizeof(header_t));
    header->is_free = 1;

    // Coalesce with Next: Merge adjacent free blocks [1]
    if (header->next && header->next->is_free) {
        header->size += sizeof(header_t) + header->next->size;
        header->next = header->next->next;
        if (header->next) header->next->prev = header;
    }

    // Coalesce with Previous
    if (header->prev && header->prev->is_free) {
        header->prev->size += sizeof(header_t) + header->size;
        header->prev->next = header->next;
        if (header->next) header->next->prev = header->prev;
    }
}