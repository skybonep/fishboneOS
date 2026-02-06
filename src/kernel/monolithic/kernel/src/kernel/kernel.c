#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <stdio.h>

#include <kernel/tty.h>
#include <drivers/serial.h>
#include <kernel/log.h>
#include <kernel/gdt.h>
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
	terminal_initialize();

	/* Newline support is left as an exercise. */
	terminal_writestring("Hello, kernel World!\n");

	printf("Printf says hello too!\n");

	/* Initialize the serial driver first */
	serial_init(SERIAL_COM1_BASE);

    /* Initialize the GDT */
    gdt_init();

	/* Log messages at various severity levels */
	kprint(LOG_DEBUG, "This is a debug message.");
	kprint(LOG_INFO, "System initialized successfully.");
	kprint(LOG_WARNING, "Low memory warning.");
	kprint(LOG_ERROR, "An error has occurred!");
	kprint(LOG_FATAL, "Fatal error! System halt.");

	log_system_info();
}
