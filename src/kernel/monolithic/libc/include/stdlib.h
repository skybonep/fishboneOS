#ifndef STDLIB_H
#define STDLIB_H

/**
 * itoa:
 * Converts an integer into a null-terminated string using the specified base.
 * @param value The integer to convert.
 * @param str The buffer where the string will be stored.
 * @param base The numerical base (2-36).
 * @return A pointer to the resulting string.
 */
char* itoa(int value, char* str, int base);

#endif