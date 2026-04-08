#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include <kernel/tty.h>
#include <kernel/menu.h>
#include <kernel/log.h>
#include <drivers/keyboard.h>
#include <drivers/serial.h>
#include "../arch/i386/vga.h"

// Direct VGA memory access for menu rendering (no serial output)
static const size_t VGA_WIDTH = 80;
static const size_t VGA_HEIGHT = 25;
static uint16_t *const VGA_MEMORY = (uint16_t *)0xB8000;

static void vga_putchar_at(char c, uint8_t color, size_t x, size_t y)
{
    if (x >= VGA_WIDTH || y >= VGA_HEIGHT)
        return;
    const size_t index = y * VGA_WIDTH + x;
    VGA_MEMORY[index] = vga_entry(c, color);
}

static void vga_writestring_at(const char *str, uint8_t color, size_t x, size_t y)
{
    size_t current_x = x;
    size_t current_y = y;

    for (size_t i = 0; str[i] != '\0'; i++)
    {
        if (str[i] == '\n')
        {
            current_x = 0;
            current_y++;
            if (current_y >= VGA_HEIGHT)
                current_y = 0;
        }
        else
        {
            vga_putchar_at(str[i], color, current_x, current_y);
            current_x++;
            if (current_x >= VGA_WIDTH)
            {
                current_x = 0;
                current_y++;
                if (current_y >= VGA_HEIGHT)
                    current_y = 0;
            }
        }
    }
}

static void vga_clear_screen(uint8_t color)
{
    for (size_t y = 0; y < VGA_HEIGHT; y++)
    {
        for (size_t x = 0; x < VGA_WIDTH; x++)
        {
            vga_putchar_at(' ', color, x, y);
        }
    }
}

// Forward declarations
static void menu_renderer_draw(const Menu *menu);
static Menu *menu_renderer_handle_input(Menu *menu);

// State tracking for optimized rendering
static const Menu *last_menu = NULL;
static size_t last_selection = 0;
static bool needs_redraw = true;
static bool callback_display_active = false; // Track if a callback result is being displayed
static Menu *callback_menu = NULL;           // Preserve the menu active during callback displays

// Temporary key input function - will be replaced with proper key code handling
static char get_key_input(void)
{
    if (keyboard_has_event())
    {
        return keyboard_get_event();
    }
    return 0;
}

Menu *menu_renderer_update(Menu *menu)
{
    if (menu == NULL)
    {
        return NULL;
    }

    // Don't redraw menu if a callback display is active
    if (!callback_display_active)
    {
        // Only redraw if menu changed or selection changed
        if (needs_redraw || last_menu != menu || last_selection != menu->current_selection)
        {
            menu_renderer_draw(menu);
            last_menu = menu;
            last_selection = menu->current_selection;
            needs_redraw = false;
        }
    }

    Menu *next_menu = menu_renderer_handle_input(menu);

    // If menu or selection changed after input, mark for redraw
    if (next_menu != menu || menu->current_selection != last_selection)
    {
        needs_redraw = true;
        callback_display_active = false; // Clear callback mode when navigating
    }

    return next_menu ? next_menu : menu;
}

static void menu_renderer_draw(const Menu *menu)
{
    // Clear screen with black background
    vga_clear_screen(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));

    // Draw title bar with equals signs padding
    const char *title = menu->title;
    size_t title_len = strlen(title);
    size_t total_width = 80;
    size_t padding = (total_width - title_len - 4) / 2; // 4 for "=== " and " ==="

    size_t current_x = 0;
    size_t current_y = 0;

    // Draw padding equals signs
    for (size_t i = 0; i < padding; i++)
    {
        vga_putchar_at('=', vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK), current_x++, current_y);
    }

    // Draw title
    vga_writestring_at("=== ", vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK), current_x, current_y);
    current_x += 4;
    vga_writestring_at(title, vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK), current_x, current_y);
    current_x += title_len;
    vga_writestring_at(" ===", vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK), current_x, current_y);
    current_x += 4;

    // Fill rest with equals signs
    size_t remaining = total_width - current_x;
    for (size_t i = 0; i < remaining; i++)
    {
        vga_putchar_at('=', vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK), current_x++, current_y);
    }

    current_y += 2; // Skip a line

    // Draw menu items
    for (size_t i = 0; i < menu->item_count; i++)
    {
        const MenuItem *item = &menu->items[i];

        uint8_t color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
        if (i == menu->current_selection)
        {
            color = vga_entry_color(VGA_COLOR_BLACK, VGA_COLOR_LIGHT_GREY);
        }

        // Draw selection indicator
        if (i == menu->current_selection)
        {
            vga_writestring_at(" > ", color, 0, current_y);
        }
        else
        {
            vga_writestring_at("   ", color, 0, current_y);
        }

        // Draw menu item label
        vga_writestring_at(item->label, color, 3, current_y);
        current_y++;
    }

    // Status bar
    current_y++;
    vga_writestring_at("================================================================================\n",
                       vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK), 0, current_y);
    current_y++;
    vga_writestring_at("Press ENTER to select, UP/DOWN to navigate, ESC to exit",
                       vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK), 0, current_y);
}

static Menu *menu_renderer_handle_input(Menu *menu)
{
    char key = get_key_input();

    if (key == 0)
    {
        return menu; // No input - return same menu
    }

    // If a callback display is active, ignore navigation and selection until ESC is pressed.
    if (callback_display_active)
    {
        if (key == KEY_ESC || key == 27)
        {
            callback_display_active = false;
            Menu *return_menu = callback_menu ? callback_menu : menu;
            callback_menu = NULL;
            needs_redraw = true; // Redraw menu on next frame after callback
            return return_menu;
        }
        return menu; // Ignore all other keys while callback content is displayed
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
        MenuItem *item = &menu->items[menu->current_selection];
        if (item->submenu != NULL)
        {
            // Set parent for proper navigation
            item->submenu->parent = menu;
            item->submenu->current_selection = 0;
            return item->submenu;
        }

        if (item->callback != NULL)
        {
            item->callback();
            callback_display_active = true; // Mark that callback display is active
            callback_menu = menu;
            // Don't return a different menu - stay here and wait for ESC
        }
        return menu;
    }
    else if (key == KEY_ESC || key == 27)
    { // ESC from menu - go to parent
        if (menu->parent != NULL)
        {
            return menu->parent;
        }
        return NULL; // Exit the root menu
    }

    return menu;
}