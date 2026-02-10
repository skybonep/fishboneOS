#include <kernel/gdt.h>

struct gdt_entry gdt[12]; // 3 is enough for basic code/data, but we can reserve more for future use
struct gdt_ptr gp;

/* Internal helper to construct entries using the bit-field structures */
void gdt_set_gate(int num, unsigned int base, unsigned int limit, 
                  struct gdt_access access, struct gdt_gran gran) 
{
    /* Split the 32-bit base address */
    gdt[num].base_low    = (base & 0xFFFF);
    gdt[num].base_middle = (base >> 16) & 0xFF;
    gdt[num].base_high   = (base >> 24) & 0xFF;

    /* Set the limit and granularity */
    gdt[num].limit_low   = (limit & 0xFFFF);
    
    /* Map the bit-field structs into the entry bytes */
    gran.limit_high      = (limit >> 16) & 0x0F;
    gdt[num].granularity = *(unsigned char*)&gran;
    gdt[num].access      = *(unsigned char*)&access;
}

void gdt_init() {
    /* 1. Set the GDT Pointer */
    gp.size = (sizeof(struct gdt_entry) * 3) - 1;
    gp.address = (unsigned int)&gdt;

    /* 2. Index 0: Null Descriptor (Hardware requirement) */
    struct gdt_access null_access = {0};
    struct gdt_gran null_gran = {0};
    gdt_set_gate(0, 0, 0, null_access, null_gran);

    /* 3. Index 1: Kernel Code Segment (Base 0, Limit 4GB, PL0, RX) */
    struct gdt_access kcode_access = {
        .present = 1,
        .dpl = 0,
        .s_type = 1,
        .executable = 1,
        .read_write = 1
    };
    struct gdt_gran kcode_gran = {
        .granularity = 1, .size = 1 // 4KB blocks and 32-bit mode
    };
    gdt_set_gate(1, 0, 0xFFFFFFFF, kcode_access, kcode_gran);

    /* 4. Index 2: Kernel Data Segment (Base 0, Limit 4GB, PL0, RW) */
    struct gdt_access kdata_access = {
        .present = 1,
        .dpl = 0,
        .s_type = 1,
        .executable = 0,
        .read_write = 1
    };
    struct gdt_gran kdata_gran = {
        .granularity = 1, .size = 1
    };
    gdt_set_gate(2, 0, 0xFFFFFFFF, kdata_access, kdata_gran);

    /* 5. Load the GDT and flush segment registers */
    extern void load_gdt(unsigned int);
    load_gdt((unsigned int)&gp);
}
