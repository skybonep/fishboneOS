#include <kernel/io.h>

#define PIC1_COMMAND 0x20
#define PIC1_DATA    0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA    0xA1

#define ICW1_INIT    0x11
#define ICW4_8086    0x01

#define PIC_EOI 0x20

void pic_remap() {
    // Save masks (optional but recommended to restore later)
    unsigned char a1 = inb(PIC1_DATA);
    unsigned char a2 = inb(PIC2_DATA);

    // ICW1: Start initialization in cascade mode
    outb(PIC1_COMMAND, ICW1_INIT);
    outb(PIC2_COMMAND, ICW1_INIT);

    // ICW2: Master PIC vector offset (0x20)
    outb(PIC1_DATA, 0x20);
    // ICW2: Slave PIC vector offset (0x28)
    outb(PIC2_DATA, 0x28);

    // ICW3: Tell Master PIC there is a slave PIC at IRQ2 (0000 0100)
    outb(PIC1_DATA, 0x04);
    // ICW3: Tell Slave PIC its cascade identity (0000 0010)
    outb(PIC2_DATA, 0x02);

    // ICW4: Set 8086/88 (MCS-80/85) mode
    outb(PIC1_DATA, ICW4_8086);
    outb(PIC2_DATA, ICW4_8086);

    // Restore masks (or set to 0 to enable all interrupts)
    outb(PIC1_DATA, a1);
    outb(PIC2_DATA, a2);
}

/* Disables every hardware interrupt on both PICs */
void pic_disable_all_irq() {
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);
}

// Enables a specific hardware interrupt by clearing its bit in the IMR.
void pic_enable_irq(unsigned char irq) {
    unsigned short port;
    
    if (irq < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq -= 8;
        
        /* Critical: Always unmask IRQ 2 on Master to allow Slave signals through */
        pic_enable_irq(2);
    }
    
    /* Read current mask, clear the bit (0 = enabled), and write it back */
    unsigned char value = inb(port) & ~(1 << irq);
    outb(port, value);
}

// send End of Interrupt signal to PICs
void pic_sendEOI(unsigned int interrupt) {
    if (interrupt < 0x20 || interrupt > 0x2F) {
        return;
    }

    if (interrupt >= 0x28 && interrupt <= 0x2F) {
        // Signal Slave PIC
        outb(PIC2_COMMAND, PIC_EOI);
    }

    // Signal Master PIC
    outb(PIC1_COMMAND, PIC_EOI);
}
