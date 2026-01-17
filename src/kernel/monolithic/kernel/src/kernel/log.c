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

/*
    * kprint:
    * A printf-like string format parsing utility for kernel diagnostics
    * It prepends a severity tag and sends the formatted output to the 
    * primary logging device (typically the serial port)
    *
    * @param level  The severity level (LOG_DEBUG, LOG_INFO, etc.).
    * @param format The format string (e.g., "Memory at %x").
    *
    * Note:
    * - This is a simplified implementation. It currently does not parse
    *   format specifiers like %s, %d, %x, etc. You can extend it as needed. (or it already does so?)
    * - Called seria_write 3 times because:
    *   Stack Constraints: If you create a large buffer (e.g., char buf) inside kprint, it sits on the Kernel Stack. If your stack is small (like the 4KB stack often used in early kernels), you risk a stack overflow if kprint is called deep within a chain of other functions.
*/
void kprint(int level, const char *format, ...) {
    // Output the tag to COM1 via serial_write
    if (level >= LOG_DEBUG && level <= LOG_FATAL) {
        serial_write(SERIAL_COM1_BASE, (char*)level_tags[level]);
    }

    // Handle formatting
    va_list args;
    va_start(args, format);

    /*
     * As you grow the project, you can add a parser here for
     * %s, %x, etc., as "functions are added on-demand"
     */
    serial_write(SERIAL_COM1_BASE, (char*)format);

    va_end(args);

    // Newline for log readability
    serial_write(SERIAL_COM1_BASE, "\n");
}