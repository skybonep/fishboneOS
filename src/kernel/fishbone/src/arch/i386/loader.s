/* 
 * Basic GRUB Multiboot Header for fishboneOS
 * 
 * This file implements the multiboot specification contract between the bootloader (GRUB)
 * and the kernel. GRUB will search for the multiboot magic number in the first 8 KiB
 * of the kernel binary, then load the kernel in 32-bit protected mode and hand control
 * to the _start entry point.
 */

/* Declare constants for the multiboot header. */
.set ALIGN,    1<<0             /* align loaded modules on page boundaries */
.set MEMINFO,  1<<1             /* provide memory map info from GRUB */
.set FLAGS,    ALIGN | MEMINFO  /* combined flags */
.set MAGIC,    0x1BADB002       /* magic number lets GRUB find the header */
.set CHECKSUM, -(MAGIC + FLAGS) /* checksum: MAGIC + FLAGS + CHECKSUM must equal 0 */

/* 
 * Multiboot Header Section
 * 
 * The bootloader searches the first 8 KiB of the kernel file for this signature.
 * The header must be 32-bit aligned (4-byte boundary) and contain:
 * 1. MAGIC (0x1BADB002) - the multiboot signature
 * 2. FLAGS - tells GRUB what services we need
 * 3. CHECKSUM - proof that this is a valid header
 */
.section .multiboot
.align 4
.long MAGIC
.long FLAGS
.long CHECKSUM

/* 
 * Kernel Stack
 * 
 * The multiboot standard does not define the stack pointer, so the kernel must
 * allocate its own stack. This reserves 16 KiB in the BSS (uninitialized data)
 * section, which means it doesn't consume space in the executable file.
 * 
 * x86 stack grows downward (decreasing addresses), so stack_top is the highest
 * address in our allocated stack region.
 * 
 * The stack must be 16-byte aligned per the System V ABI, which is required
 * for proper function call alignment and SSE instruction behavior.
 */
.section .bss
.align 16
stack_bottom:
.skip 16384  /* Reserve 16 KiB for the kernel stack */
stack_top:

/*
 * Kernel Entry Point (_start)
 * 
 * This is where GRUB hands control to the kernel. At this point:
 * - CPU is in 32-bit protected mode
 * - Interrupts are disabled
 * - Paging is disabled
 * - EAX contains the multiboot magic number (0x2BADB002)
 * - EBX contains a pointer to the multiboot info structure
 * 
 * The kernel must:
 * 1. Set up the stack pointer
 * 2. Pass the multiboot info to the C kernel entry point (kernel_main)
 * 3. Call kernel_main
 * 4. Enter an infinite loop if kernel_main returns (it shouldn't)
 */
.section .text
.global _start
.type _start, @function
_start:
	/*
	 * Set up the stack pointer
	 * 
	 * ESP must point to the top of the kernel stack. The x86 stack grows
	 * downward, so "top" means the highest address in the allocated region.
	 */
	mov $stack_top, %esp

	/*
	 * Prepare to call kernel_main(unsigned int magic, unsigned int info_ptr)
	 * 
	 * On i386, arguments are passed on the stack in right-to-left order (cdecl convention).
	 * So we push in reverse: push the second argument (EBX) first, then the first (EAX).
	 * 
	 * Current values from GRUB:
	 * - EAX = multiboot magic number (0x2BADB002)
	 * - EBX = pointer to multiboot_info structure
	 * 
	 * After pushing, the stack will look like:
	 *   [ESP]   <- info_ptr (second arg)
	 *   [ESP+4] <- magic    (first arg)
	 */
	push %ebx    /* Push multiboot info pointer (second argument) */
	push %eax    /* Push multiboot magic number (first argument) */

	/*
	 * Call the C kernel entry point
	 * 
	 * The 'call' instruction will:
	 * 1. Push the return address (0xDEADBEEF effectively - nowhere to return to)
	 * 2. Jump to kernel_main
	 * 
	 * The stack alignment requirement (16-byte aligned at function entry) is met because:
	 * - stack_top is 16-byte aligned (by .align 16 in .bss)
	 * - We've pushed 8 bytes (2 x 4 bytes) so ESP is 8-byte aligned
	 * - The 'call' instruction pushes 4 more bytes, making ESP 16-byte aligned
	 *   at the time kernel_main executes its first instruction
	 */
	call kernel_main

	/*
	 * Infinite loop - if kernel_main ever returns (it shouldn't)
	 * 
	 * Enter a halted state: disable interrupts and halt the CPU.
	 * The label '1:' is a local label used for the jump-back.
	 */
	cli           /* Clear interrupt flag - disable interrupts */
1:	hlt           /* Halt the CPU - it will only wake on interrupts (which are disabled) */
	jmp 1b        /* Jump back to halt - infinite loop */

/*
 * Mark the size of the _start function for debugging/tracing
 * (This helps debuggers and stack unwinders understand function boundaries)
 */
.size _start, . - _start
