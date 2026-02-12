#include <kernel/io.h>
#include <kernel/pic.h>
#include <kernel/log.h>


#define KBD_DATA_PORT 0x60

/* 
 * US QWERTY Scan Code Set 1 Mapping 
 * This table maps the "make" (press) codes to ASCII characters.
 * Full mapping should be derived from the tutorial mentioned in the sources.
 */
unsigned char kbd_us[] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\',
    'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' '
};

char translate_scan_code(unsigned char scan_code) {
    /* 
     * A scan code represents both presses and releases [1].
     * Typically, if the 7th bit is set (scan_code >= 0x80), it is a "break" (release) code.
     * We ignore release events for this basic implementation.
     */
    if (scan_code & 0x80) {
        return 0; 
    }

    /* Map the "make" code to ASCII using our table */
    if (scan_code < 128) {
        return kbd_us[scan_code];
    }

    return 0;
}

void itoa_temp(unsigned int num, char *str, int base) {
    int i = 0;
    do {
        str[i++] = "0123456789"[num % base];
        num /= base;
    } while (num);
    str[i] = '\0';
    // Reverse the string
    for (int j = 0; j < i / 2; j++) {
        char tmp = str[j];
        str[j] = str[i - 1 - j];
        str[i - 1 - j] = tmp;
    }
}

/**
 * keyboard_handle_interrupt:
 * Reads the scan code, translates it, and prints the character.
 */
void keyboard_handle_interrupt() {
    // 1. Read the scan code from the hardware port
    unsigned char scan_code = inb(KBD_DATA_PORT);

    char buffer[16];
    itoa_temp(scan_code, buffer, 16);

    // 2. Translate scan code to ASCII 
    // (Note: The keyboard sends scan codes, not characters)
    char c = translate_scan_code(scan_code); 

    // 3. Print the character if it's a valid keypress
    if (c != 0) {
        for (int i = 0; i < 5; i++) {
            if (buffer[i] == '\0') {
                buffer[i] = ' ';
            }
        }
        buffer[4] = c;
        buffer[5] = '\0';
        kprint(LOG_DEBUG, buffer); // Log the scan code in hex
    }
}