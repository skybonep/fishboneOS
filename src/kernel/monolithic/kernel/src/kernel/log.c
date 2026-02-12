#include <stdio.h> 
#include <kernel/log.h>
#include <drivers/serial.h>
#include <stdarg.h>

static const char *level_tags[] = {
    "[DEBUG] ",
    "[INFO] ",
    "[WARNING] ",
    "[ERROR] ",
    "[FATAL] "
};

/**
 * kprint:
 * Formats a message and writes it to the serial port (COM1).
 * Supports severity levels (DEBUG, INFO, ERROR, etc.) and printf-like formatting.
 */
void kprint(int level, const char *format, ...) {
    // 1. Declare a 1024-byte buffer on the stack for the formatted message.
    // This is a standard size for small kernel-mode log lines.
    char buf[1024]; 
    va_list args;

    // 2. Output the severity level tag (e.g., "[DEBUG] ") to the serial port.
    if (level >= LOG_DEBUG && level <= LOG_FATAL) {
        serial_write(SERIAL_COM1_BASE, (char*)level_tags[level]);
    }

    // 3. Initialize the variable argument list and pass it to the vsprintf engine.
    // This allows kprint to handle complex specifiers like %d, %x, and %b.
    va_start(args, format);
    vsprintf(buf, format, args);
    va_end(args);

    // 4. Send the fully formatted message to the serial port hardware.
    serial_write(SERIAL_COM1_BASE, buf);

    // 5. Append a newline for readability in your logging file.
    serial_write(SERIAL_COM1_BASE, "\n");
}
