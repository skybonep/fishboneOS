#ifndef IDT_H
#define IDT_H

struct idt_entry {
    unsigned short offset_low;    // Lower 16 bits of handler address
    unsigned short selector;      // Kernel Code Segment selector (0x08)
    unsigned char  zero;          // Always set to 0
    unsigned char  type_attr;     // P | DPL | 0 | D | 1 1 0
    unsigned short offset_high;   // Upper 16 bits of handler address
} __attribute__((packed));

struct idt_ptr {
    unsigned short limit;
    unsigned int   base;
} __attribute__((packed));

void idt_init(void);

#endif