#include <kernel/io.h>

/* Internal helper macros for port offsets */
#define SERIAL_DATA_PORT(base)              (base)
#define SERIAL_FIFO_CONTROL_REGISTER(base)  (base + 2)
#define SERIAL_LINE_CONTROL_REGISTER(base)  (base + 3)
#define SERIAL_MODEM_CONTROL_REGISTER(base) (base + 4)
#define SERIAL_LINE_STATUS_PORT(base)       (base + 5) // only need for receive data

/* Internal Commands */
#define SERIAL_LINE_ENABLE_DLAB_80      0x80
#define SERIAL_FIFO_CONFIG_C7           0xC7
#define SERIAL_MODEM_CONFIG_03          0x03 // send only, use 0x0B if including interrupts for receive
#define SERIAL_LINE_CONFIG_03           0x03

/**
 * serial_configure_baud_rate:
 * Sets the speed of the data being sent.
 */
void serial_configure_baud_rate(unsigned short com_base, unsigned short divisor) {
    outb(SERIAL_LINE_CONTROL_REGISTER(com_base), SERIAL_LINE_ENABLE_DLAB_80);
    outb(SERIAL_DATA_PORT(com_base), (divisor >> 8) & 0x00FF);
    outb(SERIAL_DATA_PORT(com_base), divisor & 0x00FF);
}

/**
 * serial_init:
 * Initializes the serial port and performs a loopback test.
 * Returns 0 on success, 1 if the hardware is faulty.
 */
int serial_init(unsigned short com_base) {
    // Disable all interrupts of the serial port
    outb(com_base + 1, 0x00);

    // 2. Set baud rate to 38400 (divisor = 3)
    serial_configure_baud_rate(com_base, 3);

    // 3. Configure the line: 8 bits, no parity, one stop bit
    outb(SERIAL_LINE_CONTROL_REGISTER(com_base), SERIAL_LINE_CONFIG_03);

    // 4. Configure FIFO buffers: Enable, clear, 14-byte threshold
    outb(SERIAL_FIFO_CONTROL_REGISTER(com_base), SERIAL_FIFO_CONFIG_C7);

    // 5. Configure Modem: RTS and DSR set
    outb(SERIAL_MODEM_CONTROL_REGISTER(com_base), SERIAL_MODEM_CONFIG_03);

    // 6. Loopback Test: Verify the hardware is functional
    // Set in loopback mode, bit 4 of the modem control register
    outb(SERIAL_MODEM_CONTROL_REGISTER(com_base), 0x1E);
    
    // Send a test byte
    outb(SERIAL_DATA_PORT(com_base), 0xAE);

    // Check if the serial chip returns the same byte
    if(inb(SERIAL_DATA_PORT(com_base)) != 0xAE) {
        return 1; // Faulty hardware
    }

    // 7. If success, set to normal operation mode
    outb(SERIAL_MODEM_CONTROL_REGISTER(com_base), 0x0F);
    return 0;
}

int serial_is_faulty(unsigned short com_base) {
    outb(SERIAL_MODEM_CONTROL_REGISTER(com_base), 0x1E); // Loopback mode
    outb(SERIAL_DATA_PORT(com_base), 0xAE);
    if(inb(SERIAL_DATA_PORT(com_base)) != 0xAE) {
        return 1;
    }
    return 0;
}

int serial_is_transmit_fifo_empty(unsigned short com_base)
{
    return inb(com_base + 5) & 0x20; /* Check bit 5 of Line Status Port */
}

void serial_write_char(unsigned short com_base, char a)
{
    while (serial_is_transmit_fifo_empty(com_base) == 0)
        ;              /* Wait for transmit queue to be empty */
    outb(com_base, a); /* Send the character */
}

/**
 * serial_write:
 * Higher-level function to send an entire string to the serial port.
 * @param com_port The COM port base address.
 * @param str The string to write.
 */
void serial_write(unsigned short com_base, char *str)
{
    for (int i = 0; str[i] != '\0'; i++)
    {
        serial_write_char(com_base, str[i]);
    }
}