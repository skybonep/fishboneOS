/*
 * Linker Script for fishboneOS
 * 
 * This script tells the linker how to arrange kernel object files into
 * the final executable. It controls:
 * - Entry point (_start symbol)
 * - Memory layout (where sections go)
 * - Section placement (.multiboot, .text, .data, .bss)
 * - Symbol definitions for the kernel code
 * 
 * The linker reads this script and combines all .o files according to these rules.
 */

/* Entry point - where execution starts */
ENTRY(_start)

SECTIONS
{
    /*
     * . = 2M
     * 
     * Set the current location counter to 2 MB.
     * This is a safer base for modern boot environments than 1 MB,
     * because some firmware or bootloader-reserved memory may lie below 2 MB.
     * It keeps the kernel out of the legacy lower-memory region and still
     * leaves plenty of room for the bootloader and early boot data.
     */
    . = 2M;

    /*
     * .multiboot section
     * 
     * This section contains the multiboot header, which GRUB searches for
     * in the first 8 KiB of the executable. By placing it in its own section
     * and keeping it at the start of the executable, we ensure GRUB can find it.
     * 
     * *(.multiboot) means: collect all .multiboot sections from all input files
     * into one output .multiboot section.
     */
    .multiboot : {
        *(.multiboot)
    }

    /*
     * .text section
     * 
     * This section contains all executable code:
     * - The _start function from loader.s
     * - The kernel_main function from kernel.c
     * - Any other C functions we write
     */
    .text : {
        *(.text)
    }

    /*
     * .data section
     * 
     * This section contains initialized global/static data:
     * - Global variables with initial values
     * - String literals (in some cases)
     * - Static arrays
     * 
     * The values are included in the executable file.
     */
    .data : {
        *(.data)
    }

    /*
     * .bss section
     * 
     * This section contains uninitialized global/static data:
     * - Global variables without explicit initialization
     * - Our kernel stack (allocated with .skip in loader.s)
     * 
     * The .bss section is NOT stored in the executable file.
     * The loader (GRUB) allocates the memory and zeroes it at runtime.
     * This saves space in the executable.
     */
    .bss : {
        *(.bss)
    }
}
