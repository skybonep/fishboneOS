#include <kernel/io.h>

void serial_configure(unsigned short com_port) {
    outb(com_port + 3, 0x80);    /* Enable DLAB (set baud rate divisor) */
    outb(com_port + 0, 0x03);    /* Set divisor to 3 (38400 baud) */
    outb(com_port + 1, 0x00);    /* (hi byte) */
    outb(com_port + 3, 0x03);    /* 8 bits, no parity, one stop bit */
    outb(com_port + 2, 0xC7);    /* Enable FIFO, clear with 14-byte threshold */
    outb(com_port + 4, 0x0B);    /* IRQs enabled, RTS/DSR set */
}

int serial_is_transmit_fifo_empty(unsigned short com_port) {
    return inb(com_port + 5) & 0x20; /* Check bit 5 of Line Status Port */
}

void serial_write_char(unsigned short com_port, char a) {
    while (serial_is_transmit_fifo_empty(com_port) == 0); /* Wait for transmit queue to be empty */
    outb(com_port, a);                    /* Send the character */
}

/**
 * serial_write:
 * Higher-level function to send an entire string to the serial port.
 * @param com_port The COM port base address.
 * @param str The string to write.
 */
void serial_write(unsigned short com_port, char *str) {
    for (int i = 0; str[i] != '\0'; i++) {
        serial_write_char(com_port, str[i]);
    }
}