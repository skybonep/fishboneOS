#ifndef DRIVERS_SERIAL_H
#define DRIVERS_SERIAL_H

/** 
 * Sets the speed of data transmission. 
 * @param com The COM port base address.
 * @param divisor The divisor of 115200 (e.g., 3 results in 38400 bits/s).
 */
void serial_configure_baud_rate(unsigned short com, unsigned short divisor);

/** 
 * Configures the line (8 bits, no parity, one stop bit).
 * @param com The COM port base address.
 */
void serial_configure(unsigned short com);

/** 
 * Checks if the transmit FIFO queue is empty.
 * @return 0 if not empty, 1 if empty.
 */
int serial_is_transmit_fifo_empty(unsigned int com);

/** 
 * Writes a string of data to the serial port.
 * @param com The COM port base address.
 * @param str The string to write.
 */
void serial_write(unsigned short com, char *str);

#endif /* DRIVERS_SERIAL_H */