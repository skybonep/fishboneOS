#include <stdbool.h>
#include <drivers/keyboard.h>
#include <kernel/io.h>
#include <kernel/log.h>
#include <stddef.h>

#define KBD_DATA_PORT 0x60
#define KBD_EVENT_QUEUE_SIZE 32

/*
 * US QWERTY Scan Code Set 1 Mapping
 * This table maps the "make" (press) codes to ASCII characters.
 * Full mapping should be derived from the tutorial mentioned in the sources.
 */
unsigned char kbd_us[] = {
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0, '\\',
    'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' '};

static char keyboard_queue[KBD_EVENT_QUEUE_SIZE];
static unsigned int keyboard_head = 0;
static unsigned int keyboard_tail = 0;
static unsigned int keyboard_count = 0;

char translate_scan_code(unsigned char scan_code)
{
    static bool extended_prefix = false;

    /* Handle extended key prefix first, such as arrow keys. */
    if (scan_code == 0xE0)
    {
        extended_prefix = true;
        return 0;
    }

    /*
     * A scan code represents both presses and releases [1].
     * Typically, if the 7th bit is set (scan_code >= 0x80), it is a "break" (release) code.
     * We ignore release events for this basic implementation.
     */
    if (scan_code & 0x80)
    {
        extended_prefix = false;
        return 0;
    }

    if (extended_prefix)
    {
        extended_prefix = false;
        switch (scan_code)
        {
        case 0x48:
            return KEY_UP;
        case 0x50:
            return KEY_DOWN;
        case 0x4B:
            return KEY_LEFT;
        case 0x4D:
            return KEY_RIGHT;
        default:
            return 0;
        }
    }

    /* Some QEMU keyboard injection paths may send extended arrow key codes without preserving the prefix state.
     * Fall back to arrow mapping for the common scan codes. */
    switch (scan_code)
    {
    case 0x48:
        return KEY_UP;
    case 0x50:
        return KEY_DOWN;
    case 0x4B:
        return KEY_LEFT;
    case 0x4D:
        return KEY_RIGHT;
    default:
        break;
    }

    /* Map the "make" code to ASCII using our table */
    size_t table_size = sizeof(kbd_us) / sizeof(kbd_us[0]);
    if (scan_code < table_size)
    {
        return kbd_us[scan_code];
    }

    return 0;
}

bool keyboard_has_event(void)
{
    return keyboard_count > 0;
}

char keyboard_get_event(void)
{
    if (keyboard_count == 0)
    {
        return 0;
    }

    char c = keyboard_queue[keyboard_tail];
    keyboard_tail = (keyboard_tail + 1) % KBD_EVENT_QUEUE_SIZE;
    keyboard_count--;
    return c;
}

static void keyboard_enqueue_event(char c)
{
    if (keyboard_count >= KBD_EVENT_QUEUE_SIZE)
    {
        return;
    }

    keyboard_queue[keyboard_head] = c;
    keyboard_head = (keyboard_head + 1) % KBD_EVENT_QUEUE_SIZE;
    keyboard_count++;
}

/**
 * keyboard_handle_interrupt:
 * Reads the scan code, translates it, and stores the character in a small queue.
 */
void keyboard_handle_interrupt()
{
    unsigned char scan_code = inb(KBD_DATA_PORT);
    char c = translate_scan_code(scan_code);

    if (c != 0)
    {
        keyboard_enqueue_event(c);
    }
}