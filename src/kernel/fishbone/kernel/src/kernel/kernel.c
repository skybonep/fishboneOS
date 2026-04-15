/*
 * fishboneOS Kernel Entry Point
 *
 * This is the C entry point for the kernel. It is called from loader.s (_start)
 * after the bootloader (GRUB) has set up the basic processor state.
 */

#include <stdint.h>
#include <drivers/serial.h>

/*
 * kernel_main - C entry point for the kernel
 *
 * Parameters:
 *   magic: The multiboot magic number (0x2BADB002) - passed by GRUB as validation
 *   info_ptr: Pointer to the multiboot_info structure - contains boot information
 *             from GRUB (e.g., memory map, boot device, kernel command line)
 *
 * This is called from loader.s after the bootloader has:
 * - Loaded the kernel into memory
 * - Set up 32-bit protected mode
 * - Prepared a multiboot info structure with boot data
 *
 * At this point, the kernel has:
 * - A valid stack (ESP points into our allocated stack region)
 * - Access to the full 32-bit address space
 * - Interrupts disabled by the bootloader
 * - Paging disabled
 *
 * Currently, this is just a stub that halts the CPU. In future steps,
 * we will add:
 * - Serial output setup
 * - GDT initialization
 * - IDT initialization
 * - Paging setup
 * - Memory management
 * - Task scheduling
 * - And more...
 */
void kernel_main(unsigned int magic, unsigned int info_ptr)
{
    /*
     * Verify we were called by a multiboot-compliant bootloader
     *
     * The GRUB bootloader always passes 0x2BADB002 in EAX (now passed as 'magic').
     * If magic != 0x2BADB002, something went wrong - a bootloader that doesn't
     * understand multiboot tried to boot us.
     *
     * Later, we'll add serial output to report errors here.
     * For now, just hang if the magic is wrong.
     */
    if (magic != 0x2BADB002)
    {
        /* Invalid magic - not called by a multiboot bootloader */
        /* Hang CPU - infinite loop */
        while (1)
        {
            asm volatile("hlt");
        }
    }

    /* Initialize serial port for output */
    serial_init(SERIAL_COM1_BASE);

    /* Print boot message */
    serial_write(SERIAL_COM1_BASE, "fishboneOS booting\n");

    /*
     * If we get here, we were called by GRUB with correct multiboot info.
     *
     * The info_ptr points to a multiboot_info structure that contains:
     * - Available memory (mem_lower, mem_upper)
     * - Boot device information
     * - Kernel command line (if provided)
     * - Module information (additional files loaded by GRUB)
     * - Memory map (list of RAM and reserved regions)
     * - And other boot info
     *
     * We'll use this structure in future steps for:
     * - Setting up memory management (PMM, paging)
     * - Loading modules (drivers, apps)
     * - Reading kernel parameters
     *
     * For now, we just acknowledge that we got here and halt.
     */

    /* Infinite halt - the kernel is spinning, waiting for interrupts */
    /* In later steps, this will be replaced with the main kernel loop */
    while (1)
    {
        asm volatile("hlt"); /* Halt CPU until next interrupt */
    }
}
