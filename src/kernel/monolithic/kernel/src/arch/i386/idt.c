#include <kernel/idt.h>

struct idt_entry idt[256];
struct idt_ptr idtp;

void idt_set_gate(unsigned char num, unsigned int base, unsigned short sel, unsigned char flags)
{
    idt[num].offset_low = (base & 0xFFFF);
    idt[num].offset_high = (base >> 16) & 0xFFFF;
    idt[num].selector = sel;
    idt[num].zero = 0;
    idt[num].type_attr = flags;
}

// This assembly wrapper is created in your 'cpu.s' using macros
extern void interrupt_handler_33();

void idt_init()
{
    idtp.limit = (sizeof(struct idt_entry) * 256) - 1;
    idtp.base = (unsigned int)&idt;

    // Zero out the IDT initially
    for (int i = 0; i < 256; i++)
    {
        idt_set_gate(i, 0, 0, 0);
    }

    // After setting up assembly wrappers, you will register them here:
    // idt_set_gate(0, (unsigned)interrupt_handler_0, 0x08, 0x8E);

    /* --- Register the Keyboard Handler --- */
    // Index: 33 (0x21) - Keyboard IRQ 1 remapped
    // Base: Address of the assembly wrapper 'interrupt_handler_33'
    // Selector: 0x08 - The Kernel Code Segment in your GDT
    // Flags: 0x8E - Present (1), DPL (00), 32-bit (1), Type (110)
    idt_set_gate(33, (unsigned int)interrupt_handler_33, 0x08, 0x8E);

    extern void load_idt(unsigned int);
    load_idt((unsigned int)&idtp); // Loads the IDT using the 'lidt' instruction
}