#include <drivers/keyboard.h>

#define KBD_DATA_PORT 0x60

/* Defined in Chapter 6 of the sources */
struct cpu_state
{
    unsigned int edi, esi, ebp, esp, ebx, edx, ecx, eax;
} __attribute__((packed));

struct stack_state
{
    unsigned int error_code;
    unsigned int eip;
    unsigned int cs;
    unsigned int eflags;
} __attribute__((packed));

void interrupt_handler(
    struct cpu_state cpu __attribute__((unused)),
    unsigned int interrupt,
    struct stack_state stack __attribute__((unused)))
{
    if (interrupt == 33)
    {
        keyboard_handle_interrupt();
    }
}

