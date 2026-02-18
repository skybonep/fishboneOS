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
#include <kernel/info.h>

/* Check if the compiler thinks you are targeting the wrong operating system. */
#if defined(__linux__)
#warning "You are not using a cross-compiler, you will most certainly run into trouble"
#endif

/* This tutorial will only work for the 32-bit ix86 targets. */
#if !defined(__i386__)
#warning "This tutorial needs to be compiled with a ix86-elf compiler"
#endif

void kernel_main(void)
{
	/* Initialize terminal interface */
	terminal_init();

	gdt_init();

	idt_init();

	pic_remap();

	pic_disable_all_irq();

	/* Enable the keyboard interrupt */
	pic_enable_irq(1);

	/* Enable interrupts after PIC setup */
	asm volatile("sti");

	/* Newline support is left as an exercise. */
	terminal_writestring("Hello, kernel World!\n");

	printf("Printf says hello too!\n");

	/* Initialize the serial driver first */
	serial_init(SERIAL_COM1_BASE);

	/* Log messages at various severity levels */
	printk(LOG_DEBUG, "This is a debug message. %s", "Useful for developers.");
	printk(LOG_INFO, "System initialized successfully.");
	printk(LOG_WARNING, "Low memory warning.");
	printk(LOG_ERROR, "An error has occurred!");
	printk(LOG_FATAL, "Fatal error! System halt.");

	log_system_info();

	/* Keep the kernel running to process interrupts */
	while (1)
	{
		asm volatile("hlt"); /* Halt CPU until next interrupt */
	}
}
