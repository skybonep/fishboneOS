#include <drivers/serial.h>

extern int divide_by_zero_triggered;

// Divide by zero exception handler
void divide_by_zero_handler(void)
{
    divide_by_zero_triggered = 1;
    // serial_write(SERIAL_COM1_BASE, "[ERROR] Divide by zero exception occurred!\n");
}