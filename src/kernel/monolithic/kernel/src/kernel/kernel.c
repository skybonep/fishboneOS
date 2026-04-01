#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <stdio.h>
#include <stdlib.h>

#include <kernel/tty.h>
#include <drivers/serial.h>
#include <drivers/timer.h>
#include <drivers/keyboard.h>
#include <kernel/log.h>
#include <kernel/gdt.h>
#include <kernel/idt.h>
#include <kernel/pic.h>
#include <kernel/multiboot.h>
#include <kernel/memory_map.h>
#include <kernel/pmm.h>
#include <kernel/paging.h>
#include <kernel/malloc.h>
#include <kernel/info.h>
#include <kernel/task.h>

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

static void boot_test_heap(void)
{
	printk(LOG_INFO, "--- fishboneOS Kernel Heap Test ---");

	// 1. Request memory for a 32-bit integer.
	// This tests if the heap can find or create a free chunk.
	uint32_t *test_ptr = (uint32_t *)kmalloc(sizeof(uint32_t));

	// 2. Perform a NULL check.
	// As noted in the sources, accessing unallocated memory is a "paddlin'" [2, 3].
	if (test_ptr == NULL)
	{
		printk(LOG_ERROR, "RESULT: FAILURE - kmalloc returned NULL.");
		return;
	}

	// 3. Write a distinguishable value (the "Hard Truth") to the frame [4].
	uint32_t test_value = 0xDEADBEEF;
	*test_ptr = test_value;

	// 4. Read back the value to verify the VMM translation and heap headers.
	if (*test_ptr == test_value)
	{
		printk(LOG_INFO, "RESULT: SUCCESS - Allocated at %p, Value: 0x%x", test_ptr, *test_ptr);
	}
	else
	{
		printk(LOG_ERROR, "RESULT: CORRUPTION - Expected 0x%x but found 0x%x at %p",
			   test_value, *test_ptr, test_ptr);
	}

	// 5. Free the memory to prevent a "memory leak" [5].
	// Long-running kernels must be disciplined to avoid thrashing physical memory [6, 7].
	kfree(test_ptr);
	printk(LOG_INFO, "INFO: Memory at %p has been freed.", test_ptr);
	printk(LOG_INFO, "-----------------------------------");
}

static void boot_test_pmm(void)
{
	// 1. Request a free 4 KiB frame from the PMM
	void *phys_addr = pmm_alloc_frame();

	// 2. Error Checking: Ensure we haven't run out of memory [3]
	if (phys_addr == NULL)
	{
		printk(LOG_ERROR, "PMM: Out of physical memory!");
		return;
	}

	printk(LOG_INFO, "PMM: Allocated frame at physical address 0x%08x", (uint32_t)phys_addr);

	// 3. Treat the physical address as a character pointer.
	// Because paging is disabled, we can write to this address directly [1, 6].
	char *data_ptr = (char *)phys_addr;

	// 4. Write "fishboneOS" into the allocated physical memory.
	const char *test_msg = "fishboneOS";
	int i = 0;
	while (test_msg[i] != '\0')
	{
		data_ptr[i] = test_msg[i];
		i++;
	}
	data_ptr[i] = '\0'; // Add null terminator [7].

	// 5. Read the data back from memory to verify the "Physical Reality".
	// We log the result to Bochs serial port for confirmation.
	printk(LOG_INFO, "PMM Test: Wrote to phys 0x%08x. Read back: '%s'",
		   (uint32_t)phys_addr, data_ptr);
}

static void boot_run_tests(void)
{
	boot_test_heap();
	boot_test_pmm();
	boot_test_heap();
}

static volatile uint32_t kernel_ticks = 0;
static volatile uint32_t kernel_last_service_tick = 0;

static void kernel_initialize_runtime(void)
{
	kernel_ticks = 0;
	kernel_last_service_tick = 0;
}

static void kernel_idle(void)
{
	asm volatile("hlt");
}

static void kernel_dispatch_events(void)
{
	while (keyboard_has_event())
	{
		char c = keyboard_get_event();
		terminal_putchar(c);
	}
}

static void kernel_dispatch_periodic_services(void)
{
	if (kernel_ticks != kernel_last_service_tick)
	{
		kernel_last_service_tick = kernel_ticks;
		/* Placeholder for timer-driven services such as scheduling or housekeeping. */
	}
}

static void kernel_main_loop(void)
{
	kernel_initialize_runtime();
	printk(LOG_INFO, "Kernel runtime: entering main loop");

	while (1)
	{
		kernel_dispatch_events();
		kernel_dispatch_periodic_services();
		kernel_idle();
	}
}

void kernel_main(unsigned int multiboot_magic, unsigned int multiboot_info_ptr)
{
	gdt_init();

	idt_init();

	pic_remap();

	pic_disable_all_irq();

	/* Enable the timer interrupt and keyboard interrupt */
	pic_enable_irq(0);
	pic_enable_irq(1);

	/* Initialize the serial driver first */
	serial_init(SERIAL_COM1_BASE);

	/* Initialize terminal interface */
	terminal_init();

	multiboot_info_t *mbinfo = (multiboot_info_t *)multiboot_info_ptr;
	pmm_init(mbinfo);

	paging_init();
	heap_init();
	task_init();
	printk(LOG_INFO, "Heap init: start=0x%08x next=0x%08x", KERNEL_HEAP_START, heap_get_end_vaddr());

#ifdef DEBUG
	boot_run_tests();
#endif

	/* Initialize periodic timer before enabling interrupts */
	timer_init(100);

	/* Enable interrupts after PIC setup */
	asm volatile("sti");

	uint32_t k_start = (uint32_t)&kernel_physical_start;
	uint32_t k_end = (uint32_t)&kernel_physical_end;
	printk(LOG_INFO, "Kernel image physical range: 0x%08x - 0x%08x", k_start, k_end);

#ifdef DEBUG
	log_system_info();
#endif

	multiboot_info(multiboot_magic, mbinfo);

	kernel_main_loop();
}
