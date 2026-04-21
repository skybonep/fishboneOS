#include <stdint.h>
#include <kernel/idt.h>

// IDT entries (256 entries)
struct idt_entry idt_entries[256];

// IDT pointer
struct idt_ptr idt_ptr;

// Set an IDT gate
void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags)
{
    idt_entries[num].offset_low = base & 0xFFFF;
    idt_entries[num].selector = sel;
    idt_entries[num].zero = 0;
    idt_entries[num].type_attr = flags;
    idt_entries[num].offset_high = (base >> 16) & 0xFFFF;
}

// Initialize the IDT
void idt_init(void)
{
    idt_ptr.limit = sizeof(struct idt_entry) * 256 - 1;
    idt_ptr.base = (uint32_t)&idt_entries;

    // Clear the IDT
    for (int i = 0; i < 256; i++)
    {
        idt_entries[i].offset_low = 0;
        idt_entries[i].selector = 0;
        idt_entries[i].zero = 0;
        idt_entries[i].type_attr = 0;
        idt_entries[i].offset_high = 0;
    }

    // Set up divide by zero exception (interrupt 0)
    idt_set_gate(0, (uint32_t)interrupt_handler_0, 0x08, 0x8E);

    // Load the IDT
    load_idt((unsigned int)&idt_ptr);
}

uint32_t idt_get_gate_offset(uint8_t num)
{
    return ((uint32_t)idt_entries[num].offset_high << 16) | idt_entries[num].offset_low;
}