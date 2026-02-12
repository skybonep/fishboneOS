#ifndef _STDIO_H
#define _STDIO_H

#include <stdarg.h>
#include <sys/cdefs.h>

#define EOF (-1)

#ifdef __cplusplus
extern "C" {
#endif

int putchar(int);
int puts(const char*);

/* Formats a string and prints it to the screen (framebuffer) */
int printf(const char *fmt, ...);

/**
 * sprintf:
 * Formats a string and stores it in the provided buffer 'buf'.
 * Supports: %c (char), %s (string), %d (decimal), %x (hex).
 * 
 * @param buf The destination buffer.
 * @param fmt The format string.
 * @return The number of characters written (excluding null terminator).
 */
int sprintf(char *buf, const char *fmt, ...);

/* Formats a string into a buffer using a va_list */
int vsprintf(char *buf, const char *fmt, va_list args);

#ifdef __cplusplus
}
#endif

#endif
