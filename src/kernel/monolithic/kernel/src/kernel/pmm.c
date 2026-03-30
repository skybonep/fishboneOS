#include <stdint.h>
#include <stddef.h>
#include <kernel/multiboot.h>
#include <kernel/log.h>
#include <kernel/memory_map.h>
#include <kernel/pmm.h>

/*
 * Linker labels exported from link.ld to help the PMM identify the
 * "Hard Truth" of the kernel's physical boundaries.
 */
extern uint32_t kernel_physical_start;
extern uint32_t kernel_physical_end;

#define MAX_PMM_REGIONS 64

typedef struct
{
    uint32_t start_frame;
    uint32_t end_frame;
    uint32_t type;
} physical_region_t;

static uint8_t pmm_bitmap[BITMAP_SIZE];
static physical_region_t pmm_regions[MAX_PMM_REGIONS];
static uint32_t pmm_region_count = 0;
static uint32_t total_frames = 0;

static void pmm_record_region(uint32_t start_frame, uint32_t end_frame, uint32_t type)
{
    if (pmm_region_count >= MAX_PMM_REGIONS)
        return;

    pmm_regions[pmm_region_count++] = (physical_region_t){
        .start_frame = start_frame,
        .end_frame = end_frame,
        .type = type,
    };
}

static const char *pmm_region_type_to_string(uint32_t type)
{
    switch (type)
    {
    case 0:
        return "Reserved";
    case 1:
        return "Available";
    case 2:
        return "Reserved";
    case 3:
        return "ACPI reclaimable";
    case 4:
        return "ACPI NVS";
    case 5:
        return "Bad RAM";
    default:
        return "Unknown";
    }
}

static void pmm_mark_range_free(uint32_t start_frame, uint32_t end_frame);
static void pmm_mark_range_used(uint32_t start_frame, uint32_t end_frame);

// Helper to set a bit (mark as USED)
void pmm_set_bit(uint32_t frame_idx)
{
    pmm_bitmap[frame_idx / 8] |= (1 << (frame_idx % 8));
}

// Helper to clear a bit (mark as FREE)
void pmm_clear_bit(uint32_t frame_idx)
{
    pmm_bitmap[frame_idx / 8] &= ~(1 << (frame_idx % 8));
}

// Helper to test a bit
int pmm_test_bit(uint32_t frame_idx)
{
    return pmm_bitmap[frame_idx / 8] & (1 << (frame_idx % 8));
}

static void pmm_mark_range_free(uint32_t start_frame, uint32_t end_frame)
{
    for (uint32_t i = start_frame; i <= end_frame && i < BITMAP_SIZE * 8; i++)
    {
        pmm_clear_bit(i);
    }
}

static void pmm_mark_range_used(uint32_t start_frame, uint32_t end_frame)
{
    for (uint32_t i = start_frame; i <= end_frame && i < BITMAP_SIZE * 8; i++)
    {
        pmm_set_bit(i);
    }
}

void pmm_init(multiboot_info_t *mbinfo)
{
    // 1. Initially mark EVERYTHING as reserved (1)
    for (int i = 0; i < BITMAP_SIZE; i++)
        pmm_bitmap[i] = 0xFF;

    pmm_region_count = 0;
    total_frames = 0;

    // 2. Parse the Multiboot Memory Map if present.
    if (mbinfo->flags & (1 << 6))
    {
        multiboot_memory_map_t *mmap = (multiboot_memory_map_t *)mbinfo->mmap_addr;
        while ((uint32_t)mmap < mbinfo->mmap_addr + mbinfo->mmap_length)
        {
            uint32_t start_frame = mmap->addr / PAGE_SIZE;
            uint32_t end_frame = (uint32_t)((mmap->addr + mmap->len + PAGE_SIZE - 1) / PAGE_SIZE) - 1;
            if (end_frame >= BITMAP_SIZE * 8)
                end_frame = BITMAP_SIZE * 8 - 1;

            pmm_record_region(start_frame, end_frame, mmap->type);

            if (mmap->type == 1)
            {
                pmm_mark_range_free(start_frame, end_frame);
            }

            if (end_frame + 1 > total_frames)
                total_frames = end_frame + 1;

            mmap = (multiboot_memory_map_t *)((uint32_t)mmap + mmap->size + sizeof(mmap->size));
        }
    }
    else
    {
        // Fallback only when the full mmap is unavailable.
        uint32_t top_phys = (mbinfo->mem_upper * 1024u) + 0x00100000u;
        total_frames = top_phys / PAGE_SIZE;
        if (total_frames > BITMAP_SIZE * 8)
            total_frames = BITMAP_SIZE * 8;

        pmm_record_region(0, total_frames - 1, 1);
        pmm_mark_range_free(0, total_frames - 1);
    }

    // 3. Reserve the first 1 MiB for BIOS, bootloader, and interrupt tables.
    uint32_t bios_end_frame = BIOS_BOOT_RESERVED_END / PAGE_SIZE;
    pmm_record_region(0, bios_end_frame, 0);
    pmm_mark_range_used(0, bios_end_frame);

    // 4. Reserve the kernel image.
    uint32_t k_start_frame = (uint32_t)&kernel_physical_start / PAGE_SIZE;
    uint32_t k_end_frame = ((uint32_t)&kernel_physical_end - 1) / PAGE_SIZE;
    pmm_record_region(k_start_frame, k_end_frame, 0);
    pmm_mark_range_used(k_start_frame, k_end_frame);

    // 5. Reserve the Multiboot memory map buffer if it lies in RAM.
    if (mbinfo->flags & (1 << 6))
    {
        uint32_t mmap_start_frame = mbinfo->mmap_addr / PAGE_SIZE;
        uint32_t mmap_end_frame = (mbinfo->mmap_addr + mbinfo->mmap_length - 1) / PAGE_SIZE;
        pmm_record_region(mmap_start_frame, mmap_end_frame, 0);
        pmm_mark_range_used(mmap_start_frame, mmap_end_frame);
        if (mmap_end_frame + 1 > total_frames)
            total_frames = mmap_end_frame + 1;
    }

    for (uint32_t i = 0; i < pmm_region_count; i++)
    {
        if (pmm_regions[i].end_frame + 1 > total_frames)
            total_frames = pmm_regions[i].end_frame + 1;
    }

    uint32_t free_frames = 0;
    for (uint32_t i = 0; i < total_frames; i++)
    {
        if (!pmm_test_bit(i))
            free_frames++;
    }

    printk(LOG_INFO, "PMM initialized with %d total frames, %d free, %d regions.",
           total_frames, free_frames, pmm_region_count);
    for (uint32_t i = 0; i < pmm_region_count; i++)
    {
        physical_region_t *region = &pmm_regions[i];
        const char *type_name = pmm_region_type_to_string(region->type);
        printk(LOG_INFO, "Region %u: frames %u-%u type=%u (%s)",
               i, region->start_frame, region->end_frame, region->type, type_name);
    }
}

void *pmm_alloc_frame()
{
    for (uint32_t i = 0; i < total_frames; i++)
    {
        if (!pmm_test_bit(i))
        {                                   // Found a free frame (0)
            pmm_set_bit(i);                 // Mark as USED (1)
            return (void *)(i * PAGE_SIZE); // Return physical address
        }
    }
    return NULL; // Out of memory!
}

void pmm_free_frame(void *addr)
{
    uint32_t frame_idx = (uint32_t)addr / PAGE_SIZE;
    pmm_clear_bit(frame_idx); // Mark as FREE (0)
}