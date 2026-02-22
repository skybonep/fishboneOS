#include <stdint.h>
#include <stddef.h>
#include <kernel/multiboot.h>
#include <kernel/log.h>
#include <kernel/pmm.h>

/* 
 * Linker labels exported from link.ld to help the PMM identify the 
 * "Hard Truth" of the kernel's physical boundaries.
 */
extern uint32_t kernel_physical_start;
extern uint32_t kernel_physical_end;

static uint8_t pmm_bitmap[BITMAP_SIZE];
static uint32_t total_frames = 0;

// Helper to set a bit (mark as USED)
void pmm_set_bit(uint32_t frame_idx) {
    pmm_bitmap[frame_idx / 8] |= (1 << (frame_idx % 8));
}

// Helper to clear a bit (mark as FREE)
void pmm_clear_bit(uint32_t frame_idx) {
    pmm_bitmap[frame_idx / 8] &= ~(1 << (frame_idx % 8));
}

// Helper to test a bit
int pmm_test_bit(uint32_t frame_idx) {
    return pmm_bitmap[frame_idx / 8] & (1 << (frame_idx % 8));
}

void pmm_init(multiboot_info_t *mbinfo) {
    // 1. Initially mark EVERYTHING as reserved (1)
    for (int i = 0; i < BITMAP_SIZE; i++) pmm_bitmap[i] = 0xFF;

    // 2. Determine total frames based on Upper Memory
    total_frames = (mbinfo->mem_upper * 1024) / PAGE_SIZE;

    // 3. Parse the Multiboot Memory Map
    multiboot_memory_map_t *mmap = (multiboot_memory_map_t *)mbinfo->mmap_addr;
    while ((uint32_t)mmap < mbinfo->mmap_addr + mbinfo->mmap_length) {
        if (mmap->type == 1) { // Available RAM
            uint32_t start_frame = mmap->addr / PAGE_SIZE;
            uint32_t end_frame = (mmap->addr + mmap->len) / PAGE_SIZE;
            
            for (uint32_t i = start_frame; i < end_frame; i++) {
                pmm_clear_bit(i); // Mark as FREE (0)
            }
        }
        mmap = (multiboot_memory_map_t *)((uint32_t)mmap + mmap->size + sizeof(mmap->size));
    }

    // 4. THE HARD TRUTH: Protect the Kernel
    uint32_t k_start_frame = (uint32_t)&kernel_physical_start / PAGE_SIZE;
    uint32_t k_end_frame = (uint32_t)&kernel_physical_end / PAGE_SIZE;
    for (uint32_t i = k_start_frame; i <= k_end_frame; i++) {
        pmm_set_bit(i); // Mark as USED (1)
    }
    
    // 5. Protect the first 1MB (BIOS/GRUB/IVT) for safety
    for (uint32_t i = 0; i < 256; i++) pmm_set_bit(i); 

    printk(LOG_INFO, "PMM initialized with %d frames.", total_frames);
}

void *pmm_alloc_frame() {
    for (uint32_t i = 0; i < total_frames; i++) {
        if (!pmm_test_bit(i)) { // Found a free frame (0)
            pmm_set_bit(i);     // Mark as USED (1)
            return (void *)(i * PAGE_SIZE); // Return physical address
        }
    }
    return NULL; // Out of memory!
}

void pmm_free_frame(void *addr) {
    uint32_t frame_idx = (uint32_t)addr / PAGE_SIZE;
    pmm_clear_bit(frame_idx); // Mark as FREE (0)
}