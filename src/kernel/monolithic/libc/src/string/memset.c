#include <stddef.h>

/**
 * memset:
 * Fills the first 'n' bytes of the memory area pointed to by 's' 
 * with the constant byte 'c'.
 */
void *memset(void *s, int c, size_t n) {
    unsigned char *p = (unsigned char *)s;
    while (n--) {
        *p++ = (unsigned char)c;
    }
    return s;
}