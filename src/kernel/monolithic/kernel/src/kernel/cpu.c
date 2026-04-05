#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <kernel/cpu.h>

bool cpu_has_cpuid(void)
{
    unsigned int original_flags;
    unsigned int modified_flags __attribute__((unused));
    unsigned int after_flags;

    __asm__ volatile(
        "pushfl\n\t"
        "pop %0\n\t"
        "mov %0, %1\n\t"
        "xor $0x200000, %1\n\t"
        "push %1\n\t"
        "popfl\n\t"
        "pushfl\n\t"
        "pop %1\n\t"
        "push %0\n\t"
        "popfl\n\t"
        : "=r"(original_flags), "=r"(after_flags)
        :
        : "memory");

    return ((original_flags ^ after_flags) & 0x200000u) != 0;
}

void cpu_cpuid(unsigned int function, unsigned int *eax, unsigned int *ebx, unsigned int *ecx, unsigned int *edx)
{
    unsigned int a, b, c, d;

    __asm__ volatile("cpuid"
                     : "=a"(a), "=b"(b), "=c"(c), "=d"(d)
                     : "a"(function));

    if (eax)
        *eax = a;
    if (ebx)
        *ebx = b;
    if (ecx)
        *ecx = c;
    if (edx)
        *edx = d;
}

void cpu_get_vendor_string(char *vendor_out)
{
    if (vendor_out == NULL)
    {
        return;
    }

    if (!cpu_has_cpuid())
    {
        vendor_out[0] = '\0';
        return;
    }

    unsigned int eax, ebx, ecx, edx;
    cpu_cpuid(0, &eax, &ebx, &ecx, &edx);

    ((unsigned int *)vendor_out)[0] = ebx;
    ((unsigned int *)vendor_out)[1] = edx;
    ((unsigned int *)vendor_out)[2] = ecx;
    vendor_out[12] = '\0';
}
