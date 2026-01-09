#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <stdio.h>

#include <kernel/tty.h>
#include <drivers/serial.h>

#define SERIAL_COM1_BASE 0x3F8

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
    serial_configure(SERIAL_COM1_BASE);

    /* Send a debug message to the emulator */
    serial_write(SERIAL_COM1_BASE, "DEBUG: fishboneOS has successfully initialized serial logging.\n");

}
