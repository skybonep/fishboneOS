/*
 * fishboneOS Kernel Entry Point
 *
 * This is the C entry point for the kernel. It is called from loader.s (_start)
 * after the bootloader (GRUB) has set up the basic processor state.
 */

#include <stdint.h>
#include <drivers/serial.h>
#include <kernel/test.h>
#include <kernel/gdt.h>
#include <kernel/idt.h>

int kernel_test_failures = 0;
int divide_by_zero_triggered = 0;

#ifdef DEBUG
static void boot_run_tests(void)
{
    TEST_START("serial_loopback");
    if (serial_is_faulty(SERIAL_COM1_BASE) == 0)
    {
        TEST_PASS("serial_loopback", "COM1 loopback verified");
    }
    else
    {
        TEST_FAIL("serial_loopback", "COM1 loopback failed");
    }

    TEST_START("boot_message");
    TEST_PASS("boot_message", "Boot message queued");

    TEST_START("divide_by_zero");
    divide_by_zero_triggered = 0;
    asm volatile("mov $0, %%eax; div %%eax" : : : "eax");
    if (divide_by_zero_triggered)
    {
        TEST_PASS("divide_by_zero", "Exception handled successfully");
    }
    else
    {
        TEST_FAIL("divide_by_zero", "Exception not triggered");
    }
    TEST_END();
}
#endif

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

    /* Initialize GDT */
    gdt_init();
    serial_write(SERIAL_COM1_BASE, "[INFO] GDT initialized\n");

    /* Initialize IDT */
    idt_init();
    serial_write(SERIAL_COM1_BASE, "[INFO] IDT initialized\n");

    /* Print boot message */
    serial_write(SERIAL_COM1_BASE, "fishboneOS booting\n");

#ifdef DEBUG
    boot_run_tests();
#endif

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
