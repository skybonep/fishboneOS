#ifndef IDT_H
#define IDT_H

#include <stdint.h>

// IDT entry structure (8 bytes)
struct idt_entry
{
    uint16_t offset_low;  // Lower 16 bits of handler address
    uint16_t selector;    // Kernel segment selector
    uint8_t zero;         // Always 0
    uint8_t type_attr;    // Type and attributes
    uint16_t offset_high; // Upper 16 bits of handler address
} __attribute__((packed));

// IDT pointer structure
struct idt_ptr
{
    uint16_t limit; // Size of IDT - 1
    uint32_t base;  // Base address of IDT
} __attribute__((packed));

// Interrupt handler function type
typedef void (*interrupt_handler_t)(void);

// Function prototypes
void idt_init(void);
void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags);
uint32_t idt_get_gate_offset(uint8_t num);

// Extern declarations for assembly functions
extern void load_idt(unsigned int);
extern void interrupt_handler_0(void);

// IntelliSense stub definitions for assembly-defined functions
// These are only used by the editor and are not compiled by the actual compiler
#ifdef __INTELLISENSE__
void interrupt_handler_0(void) {}
void load_idt(unsigned int idt_ptr) {}
#endif

#endif // IDT_H