#ifndef KERNEL_MENU_RENDERER_H
#define KERNEL_MENU_RENDERER_H

#include <kernel/menu.h>

// Update the menu renderer for one frame
// Returns true if the menu should exit, false otherwise
bool menu_renderer_update(Menu *menu);

#endif