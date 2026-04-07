#ifndef KERNEL_MENU_H
#define KERNEL_MENU_H

#include <stdbool.h>
#include <stddef.h>

// Forward declaration
struct Menu;

// Menu item callback function type
typedef void (*menu_callback_t)(void);

// Menu item structure
typedef struct MenuItem
{
    const char *label;        // Display text for the menu item
    menu_callback_t callback; // Function to call when selected (NULL for submenus)
    struct Menu *submenu;     // Pointer to submenu (NULL for leaf items)
    bool enabled;             // Whether this item is selectable
} MenuItem;

// Menu structure
typedef struct Menu
{
    const char *title;        // Menu title (displayed in top border)
    MenuItem *items;          // Array of menu items
    size_t item_count;        // Number of items in the array
    size_t current_selection; // Index of currently selected item
    struct Menu *parent;      // Pointer to parent menu (NULL for root)
} Menu;

#endif