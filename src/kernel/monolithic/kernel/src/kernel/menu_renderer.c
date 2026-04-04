#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include <kernel/tty.h>
#include <kernel/menu.h>
#include <kernel/log.h>
#include <drivers/keyboard.h>
#include <drivers/serial.h>
#include "../arch/i386/vga.h"

// Forward declarations
static void menu_renderer_draw(const Menu *menu);
static bool menu_renderer_handle_input(Menu *menu);

// Temporary key input function - will be replaced with proper key code handling
static char get_key_input(void)
{
    if (keyboard_has_event())
    {
        return keyboard_get_event();
    }
    return 0;
}

bool menu_renderer_update(Menu *menu)
{
    if (menu == NULL)
    {
        return true;
    }

    menu_renderer_draw(menu);

    if (menu_renderer_handle_input(menu))
    {
        return true; // Exit menu
    }

    return false;
}

static void menu_renderer_draw(const Menu *menu)
{
    // Clear screen
    terminal_init();

    // Draw title bar with equals signs padding
    const char *title = menu->title;
    size_t title_len = strlen(title);
    size_t total_width = 80;
    size_t padding = (total_width - title_len - 4) / 2; // 4 for "=== " and " ==="

    // Draw padding equals signs
    for (size_t i = 0; i < padding; i++)
    {
        terminal_putchar('=');
    }

    // Draw title
    terminal_writestring("=== ");
    terminal_writestring(title);
    terminal_writestring(" ===");

    // Fill rest with equals signs
    size_t remaining = total_width - (padding + 4 + title_len + 4);
    for (size_t i = 0; i < remaining; i++)
    {
        terminal_putchar('=');
    }

    terminal_putchar('\n');
    terminal_putchar('\n');

    // Draw menu items
    for (size_t i = 0; i < menu->item_count; i++)
    {
        const MenuItem *item = &menu->items[i];

        if (i == menu->current_selection)
        {
            // Highlighted item (inverse colors)
            terminal_setcolor(vga_entry_color(VGA_COLOR_BLACK, VGA_COLOR_LIGHT_GREY));
            terminal_writestring(" > ");
        }
        else
        {
            // Normal item
            terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
            terminal_writestring("   ");
        }

        terminal_writestring(item->label);
        terminal_putchar('\n');
    }

    // Reset color
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));

    // Status bar
    terminal_putchar('\n');
    terminal_writestring("================================================================================\n");
    terminal_writestring("Press ENTER to select, UP/DOWN to navigate, ESC to exit\n");
}

static bool menu_renderer_handle_input(Menu *menu)
{
    char key = get_key_input();

    if (key == 0)
    {
        return false; // No input
    }

    // Support both arrow keys and WASD navigation.
    if (key == KEY_UP || key == KEY_LEFT || key == 'w' || key == 'W' || key == 'a' || key == 'A')
    { // Up/Left
        if (menu->current_selection > 0)
        {
            menu->current_selection--;
        }
    }
    else if (key == KEY_DOWN || key == KEY_RIGHT || key == 's' || key == 'S' || key == 'd' || key == 'D')
    { // Down/Right
        if (menu->current_selection < menu->item_count - 1)
        {
            menu->current_selection++;
        }
    }
    else if (key == KEY_ENTER || key == '\n' || key == '\r')
    { // Enter
        const MenuItem *item = &menu->items[menu->current_selection];
        if (item->callback != NULL)
        {
            item->callback();
        }
        // TODO: Handle submenu navigation
    }
    else if (key == KEY_ESC || key == 27)
    {                // ESC
        return true; // Exit menu
    }

    return false;
}