#ifndef DRIVERS_KEYBOARD_H
#define DRIVERS_KEYBOARD_H

#include <stdbool.h>

/**
 * keyboard_handle_interrupt:
 * Processes the hardware signal from the keyboard.
 */
void keyboard_handle_interrupt(void);

bool keyboard_has_event(void);
char keyboard_get_event(void);

#endif