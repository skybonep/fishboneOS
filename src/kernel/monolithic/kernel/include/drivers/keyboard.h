#ifndef DRIVERS_KEYBOARD_H
#define DRIVERS_KEYBOARD_H

#include <stdbool.h>

// Key code constants (extended ASCII)
#define KEY_UP 72
#define KEY_DOWN 80
#define KEY_LEFT 75
#define KEY_RIGHT 77
#define KEY_ENTER 13
#define KEY_ESC 27

/**
 * keyboard_handle_interrupt:
 * Processes the hardware signal from the keyboard.
 */
void keyboard_handle_interrupt(void);

bool keyboard_has_event(void);
char keyboard_get_event(void);

#endif