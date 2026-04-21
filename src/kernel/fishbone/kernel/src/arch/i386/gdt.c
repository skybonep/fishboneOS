#include <kernel/gdt.h>

// Define size_t
typedef unsigned int size_t;

// GDT entries (6 entries)
struct gdt_entry gdt_entries[6];

// GDT pointer
struct gdt_ptr gdt_ptr;

// Simple memset implementation
void *memset(void *s, int c, size_t n)
{
    unsigned char *p = s;
    while (n--)
    {
        *p++ = c;
    }
    return s;
}

// Extern declarations for assembly functions
extern void load_gdt(unsigned int);
extern void load_tss(void);

// Set a GDT gate
void gdt_set_gate(int num, unsigned long base, unsigned long limit, unsigned char access, unsigned char gran)
{
    gdt_entries[num].base_low = (base & 0xFFFF);
    gdt_entries[num].base_middle = (base >> 16) & 0xFF;
    gdt_entries[num].base_high = (base >> 24) & 0xFF;

    gdt_entries[num].limit_low = (limit & 0xFFFF);
    gdt_entries[num].granularity = ((limit >> 16) & 0x0F) | (gran & 0xF0);
    gdt_entries[num].access = access;
}

// Initialize the GDT
void gdt_init(void)
{
    gdt_ptr.size = (sizeof(struct gdt_entry) * 6) - 1;
    gdt_ptr.address = (unsigned int)&gdt_entries;

    memset(&gdt_entries, 0, sizeof(struct gdt_entry) * 6);

    gdt_set_gate(0, 0, 0, 0, 0);                // Null segment
    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF); // Code segment
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF); // Data segment
    gdt_set_gate(3, 0, 0xFFFFFFFF, 0xFA, 0xCF); // User mode code segment
    gdt_set_gate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF); // User mode data segment

    // TSS (Task State Segment) - we'll set it up later
    gdt_set_gate(5, 0, 0, 0x89, 0x00);

    load_gdt((unsigned int)&gdt_ptr);
    load_tss();
}