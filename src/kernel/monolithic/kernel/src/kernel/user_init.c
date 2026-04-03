#include <stdint.h>
#include <stddef.h>

#include <kernel/gdt.h>
#include <kernel/syscall.h>

void user_main(void)
{
    asm volatile(
        "movw %0, %%ax\n\t"
        "movw %%ax, %%ds\n\t"
        "movw %%ax, %%es\n\t"
        "movw %%ax, %%fs\n\t"
        "movw %%ax, %%gs\n\t"
        :
        : "i"(USER_DATA_SEG)
        : "ax");

    const char message[] = "hello from user mode!\n";

    register uint32_t eax asm("eax") = SYS_WRITE;
    register uint32_t ebx asm("ebx") = 2; // Use serial output to see from qemu -serial stdio
    register const char *ecx asm("ecx") = message;
    register uint32_t edx asm("edx") = (uint32_t)(sizeof(message) - 1);
    asm volatile("int $0x80"
                 : "+a"(eax)
                 : "b"(ebx), "c"(ecx), "d"(edx)
                 : "memory");

    register uint32_t exit_eax asm("eax") = SYS_EXIT;
    register uint32_t exit_ebx asm("ebx") = 0;
    asm volatile("int $0x80"
                 : "+a"(exit_eax)
                 : "b"(exit_ebx)
                 : "memory");

    for (;;)
    {
        asm volatile("hlt");
    }
}
