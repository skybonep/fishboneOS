#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <stdio.h>
#include <stdlib.h>

#include <kernel/tty.h>
#include <drivers/serial.h>
#include <kernel/log.h>
#include <kernel/gdt.h>
#include <kernel/idt.h>
#include <kernel/pic.h>
#include <kernel/multiboot.h>
#include <kernel/info.h>

/* Check if the compiler thinks you are targeting the wrong operating system. */
#if defined(__linux__)
#warning "You are not using a cross-compiler, you will most certainly run into trouble"
#endif

/* This tutorial will only work for the 32-bit ix86 targets. */
#if !defined(__i386__)
#warning "This tutorial needs to be compiled with a ix86-elf compiler"
#endif

// Access the linker labels as "functions" to get their addresses
extern void kernel_physical_start(void);
extern void kernel_physical_end(void);

void kernel_main(unsigned int multiboot_magic, unsigned int multiboot_info_ptr)
// void kernel_main(uint32_t mb_ptr)
{
	gdt_init();

	idt_init();

	pic_remap();

	pic_disable_all_irq();

	/* Enable the keyboard interrupt */
	pic_enable_irq(1);

	/* Enable interrupts after PIC setup */
	asm volatile("sti");

	/* Initialize the serial driver first */
	serial_init(SERIAL_COM1_BASE);

	/* Initialize terminal interface */
	terminal_init();

	// Log kernel boundaries (todo: this is temporary until we implement a proper memory map)
	uint32_t k_start = (uint32_t)&kernel_physical_start;
	uint32_t k_end = (uint32_t)&kernel_physical_end;
	printk(LOG_INFO, "Kernel resides at: 0x%08x - 0x%08x", k_start, k_end);

	log_system_info();

	multiboot_info_t *mbinfo = (multiboot_info_t *)multiboot_info_ptr;
	multiboot_info(multiboot_magic, mbinfo);

	/* Keep the kernel running to process interrupts */
	while (1)
	{
		asm volatile("hlt"); /* Halt CPU until next interrupt */
	}
}
