#include <stdint.h>
#include <stddef.h>
#include <kernel/gdt.h>

// User-mode Hello World task
void hello_world_main(void)
{
    // Initialize user-mode data segments (required for user tasks)
    asm volatile(
        "movw %0, %%ax\n\t"
        "movw %%ax, %%ds\n\t"
        "movw %%ax, %%es\n\t"
        "movw %%ax, %%fs\n\t"
        "movw %%ax, %%gs\n\t"
        :
        : "i"(USER_DATA_SEG)
        : "ax");

    // Hello World message
    const char message[] = "Hello World from user task!\n";

    // Syscall: write to serial console (fd=2)
    register uint32_t eax asm("eax") = 1; // SYS_WRITE
    register uint32_t ebx asm("ebx") = 2; // fd=2 (serial)
    register const char *ecx asm("ecx") = message;
    register uint32_t edx asm("edx") = sizeof(message) - 1;

    asm volatile("int $0x80"
                 : "+a"(eax)
                 : "b"(ebx), "c"(ecx), "d"(edx)
                 : "memory");

    // Exit syscall
    register uint32_t exit_eax asm("eax") = 2; // SYS_EXIT
    register uint32_t exit_ebx asm("ebx") = 0; // status=0

    asm volatile("int $0x80"
                 : "+a"(exit_eax)
                 : "b"(exit_ebx)
                 : "memory");

    // Should never reach here
    for (;;)
    {
        asm volatile("hlt");
    }
}