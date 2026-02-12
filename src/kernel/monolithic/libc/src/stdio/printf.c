#include <stdarg.h>
#include <stdio.h>

/**
 * printf:
 * Formats a string and outputs it to the terminal using putchar().
 * 
 * @param fmt The format string.
 * @return The number of characters written.
 */
int printf(const char *fmt, ...) {
    // We use a buffer to store the formatted string before output.
    // 1024 bytes is a common limit for kernel-mode log lines.
    char buf[1024]; 
    va_list args;
    
    va_start(args, fmt);
    // Reuse the core vsprintf engine we created earlier.
    int len = vsprintf(buf, fmt, args);
    va_end(args);

    // Since you don't have fb_write yet, we iterate through the
    // formatted buffer and call your provided putchar() for each character.
    for (int i = 0; i < len; i++) {
        putchar(buf[i]);
    }

    return len;
}
