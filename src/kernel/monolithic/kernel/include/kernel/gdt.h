#ifndef GDT_H
#define GDT_H

/* Access Byte bit-fields (8 bits total) [6, 7] */
struct gdt_access {
    unsigned char accessed   : 1; // Bit 0
    unsigned char read_write : 1; // Bit 1: 1 for Readable(Code)/Writable(Data)
    unsigned char conforming : 1; // Bit 2: Direction bit/Conforming bit
    unsigned char executable : 1; // Bit 3: 1 for Code, 0 for Data
    unsigned char s_type     : 1; // Bit 4: 1 for Code/Data, 0 for System
    unsigned char dpl        : 2; // Bits 5-6: Privilege Level (0-3)
    unsigned char present    : 1; // Bit 7: Must be 1 for valid segments
} __attribute__((packed));

/* Granularity/Flags bit-fields (8 bits total) [8] */
struct gdt_gran {
    unsigned char limit_high  : 4; // Bits 0-3: The highest 4 bits of the limit
    unsigned char available   : 1; // Bit 4: Available for system use
    unsigned char long_mode   : 1; // Bit 5: 1 for 64-bit code segment
    unsigned char size        : 1; // Bit 6: 1 for 32-bit, 0 for 16-bit
    unsigned char granularity : 1; // Bit 7: 1 for 4KB blocks, 0 for bytes
} __attribute__((packed));

/* An 8-byte entry in the GDT [8, 9] */
struct gdt_entry {
    unsigned short limit_low;     /* The lower 16 bits of the limit */
    unsigned short base_low;      /* The lower 16 bits of the base */
    unsigned char  base_middle;   /* The next 8 bits of the base */
    unsigned char  access;        /* Access flags (DPL, Type, etc.) [10] */
    unsigned char  granularity;   /* Granularity and the high 4 bits of the limit */
    unsigned char  base_high;     /* The last 8 bits of the base */
} __attribute__((packed));

/* The 6-byte GDT Pointer required by the lgdt instruction [4] */
struct gdt_ptr {
    unsigned short size;         /* The size of the GDT minus 1 */
    unsigned int   address;          /* The linear address of the GDT array */
} __attribute__((packed));

void gdt_init(void);

#endif
