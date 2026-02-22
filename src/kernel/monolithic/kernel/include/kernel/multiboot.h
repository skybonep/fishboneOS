#ifndef KERNEL_MULTIBOOT_H
#define KERNEL_MULTIBOOT_H

#include <stdint.h>

typedef struct {
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;

    /* 
     * The ELF section header table information.
     * These 4 fields (16 bytes) must be present so that the 
     * compiler finds mmap_length at the correct offset.
     */
    uint32_t elf_sh_num;
    uint32_t elf_sh_size;
    uint32_t elf_sh_addr;
    uint32_t elf_sh_shndx;

    uint32_t mmap_length;
    uint32_t mmap_addr;

    /* ... other fields like drives_addr can follow ... */
} __attribute__((packed)) multiboot_info_t;

typedef struct {
    uint32_t size;
    uint64_t addr;
    uint64_t len;
    uint32_t type;
} __attribute__((packed)) multiboot_memory_map_t;

#endif /* MULTIBOOT_H */