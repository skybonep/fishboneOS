#ifndef KERNEL_LOG_H
#define KERNEL_LOG_H

/**
 * Log levels to distinguish the severeness of messages
 */
#define LOG_DEBUG 0
#define LOG_INFO 1
#define LOG_WARNING 2
#define LOG_ERROR 3
#define LOG_FATAL 4

/**
 * kprint:
 * A printf-like string format parsing utility for kernel diagnostics
 * It prepends a severity tag and sends the formatted output to the
 * primary logging device (typically the serial port)
 *
 * @param level  The severity level (LOG_DEBUG, LOG_INFO, etc.).
 * @param format The format string (e.g., "Memory at %x").
 */
void printk(int level, const char *format, ...);

#endif /* KERNEL_LOG_H */