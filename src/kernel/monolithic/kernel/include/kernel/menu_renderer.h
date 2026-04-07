#ifndef KERNEL_MENU_RENDERER_H
#define KERNEL_MENU_RENDERER_H

#include <kernel/menu.h>

// Update the menu renderer for one frame
// Returns the active menu after the update.
// Returns NULL if the menu system should exit.
Menu *menu_renderer_update(Menu *menu);

#endif