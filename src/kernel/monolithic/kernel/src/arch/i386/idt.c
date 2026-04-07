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

// These assembly wrappers are created in your 'cpu.s' using macros
extern void interrupt_handler_32();
extern void interrupt_handler_33();
extern void interrupt_handler_14();
extern void interrupt_handler_128();

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

    /* --- Register the Timer Handler --- */
    // Index: 32 (0x20) - Timer IRQ 0 remapped
    // Base: Address of the assembly wrapper 'interrupt_handler_32'
    idt_set_gate(32, (unsigned int)interrupt_handler_32, 0x08, 0x8E);

    /* --- Register the Keyboard Handler --- */
    // Index: 33 (0x21) - Keyboard IRQ 1 remapped
    // Base: Address of the assembly wrapper 'interrupt_handler_33'
    idt_set_gate(33, (unsigned int)interrupt_handler_33, 0x08, 0x8E);

    /* --- Register the Page Fault Handler --- */
    // Index: 14 - Page Fault
    idt_set_gate(14, (unsigned int)interrupt_handler_14, 0x08, 0x8E);

    /* --- Register the Syscall Handler --- */
    // Index: 128 (0x80) - User syscall trap
    // DPL=3 allows ring-3 code to execute INT 0x80
    idt_set_gate(128, (unsigned int)interrupt_handler_128, 0x08, 0xEE);

    extern void load_idt(unsigned int);
    load_idt((unsigned int)&idtp); // Loads the IDT using the 'lidt' instruction
}

unsigned int idt_get_used_entries(void)
{
    unsigned int count = 0;
    for (int i = 0; i < 256; i++)
    {
        if (idt[i].offset_low != 0 || idt[i].offset_high != 0)
        {
            count++;
        }
    }
    return count;
}

void idt_get_configured_interrupts(unsigned int *interrupts, unsigned int max_count, unsigned int *count)
{
    unsigned int found = 0;
    for (int i = 0; i < 256 && found < max_count; i++)
    {
        if (idt[i].offset_low != 0 || idt[i].offset_high != 0)
        {
            interrupts[found++] = i;
        }
    }
    *count = found;
}