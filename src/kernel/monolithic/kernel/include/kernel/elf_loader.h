#ifndef KERNEL_ELF_LOADER_H
#define KERNEL_ELF_LOADER_H

#include <stddef.h>
#include <stdint.h>

#define MAX_USER_CODE_PAGES 16

/**
 * load_elf:
 *   Loads a user ELF binary into the specified user page directory.
 *   The loaded segments are mapped into the user virtual address space.
 *   Returns 0 on success and fills entry_point with the virtual entry address.
 *   page_frames receives the physical frames allocated for the loaded segments.
 *   page_count receives the number of mapped pages.
 */
int load_elf(const void *elf_data,
             size_t elf_size,
             uint32_t user_pdt,
             uint32_t *entry_point,
             uint32_t *page_frames,
             uint32_t *page_count);

#endif /* KERNEL_ELF_LOADER_H */
