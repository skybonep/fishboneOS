#ifndef KERNEL_MEMORY_MAP_H
#define KERNEL_MEMORY_MAP_H

#include <stdint.h>

/*
 * Physical memory layout for the monolithic kernel.
 */
#define BIOS_BOOT_RESERVED_START 0x00000000
#define BIOS_BOOT_RESERVED_END 0x000FFFFF

#define IDENTITY_MAP_START 0x00000000
#define IDENTITY_MAP_END 0x003FFFFF

#define PHYS_KERNEL_BASE 0x00200000
#define PHYS_KERNEL_END 0x003FFFFF

/*
 * Higher-half kernel virtual layout.
 */
#define KERNEL_VIRT_BASE 0xC0000000
#define KERNEL_HEAP_START 0xC0400000
#define KERNEL_HEAP_END 0xC07FFFFF

/*
 * Reserved kernel stack region in the higher-half.
 * This leaves room between the kernel image and the heap,
 * while avoiding the temporary VMM mapping page.
 */
#define KERNEL_STACK_REGION_START 0xC0200000
#define KERNEL_STACK_REGION_END 0xC03FEFFF
#define KERNEL_STACK_SIZE 0x00004000

/*
 * VMM/recursive paging helpers.
 */
#define VMM_PDT_VIRTUAL_ADDR 0xFFFFF000
#define VMM_TEMP_PAGE 0xC03FF000

/*
 * Reserved address spaces.
 */
#define MMIO_RESERVED_START 0xF0000000
#define USER_SPACE_START 0x00000000
#define USER_SPACE_END 0xBFFFFFFF

#define USER_HEAP_START 0x00800000
#define USER_HEAP_END 0x00A00000

#endif /* KERNEL_MEMORY_MAP_H */
