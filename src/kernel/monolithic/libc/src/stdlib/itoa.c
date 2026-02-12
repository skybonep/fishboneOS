#include <stdint.h> // Using fixed-width types for precision [Think OS context]

char* itoa(int value, char* str, int base) {
    char *ptr = str, *ptr1 = str, tmp_char;
    int tmp_value;

    // 1. Check for supported bases
    if (base < 2 || base > 36) {
        *str = '\0';
        return str;
    }

    // 2. Handle negative numbers for decimal (base 10)
    // Non-decimal bases are typically treated as unsigned bit vectors
    tmp_value = value;
    if (tmp_value < 0 && base == 10) {
        *ptr++ = '-';
        ptr1++; // Mark the start of digits for the reversal step
        tmp_value = -tmp_value;
    }

    // 3. Convert value to characters (generated in reverse order)
    // We use a do-while loop to ensure '0' is handled correctly [Think OS context]
    do {
        int remainder = tmp_value % base;
        // Map remainder to ASCII using a lookup table
        // '0'-'9' maps to 48-57; 'a'-'z' maps to 97-122 [7, 8]
        *ptr++ = "0123456789abcdefghijklmnopqrstuvwxyz" [remainder < 0 ? -remainder : remainder];
        tmp_value /= base;
    } while (tmp_value != 0);

    // 4. Null-terminate the string
    *ptr-- = '\0';

    // 5. Reverse the string in-place
    // Since digits were added from least to most significant, we must flip them
    while (ptr1 < ptr) {
        tmp_char = *ptr;
        *ptr-- = *ptr1;
        *ptr1++ = tmp_char;
    }

    return str;
}