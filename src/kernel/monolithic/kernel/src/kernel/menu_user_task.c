#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include <kernel/user_syscalls.h>
#include <kernel/menu.h>

// Simple strlen implementation
static size_t user_strlen(const char *str)
{
    if (str == NULL)
        return 0;
    size_t len = 0;
    while (str[len] != '\0')
        len++;
    return len;
}

// Simple TTY output functions using syscalls
static void user_terminal_putchar(char c)
{
    user_write(2, &c, 1); // Write to stderr (serial)
}

static void user_terminal_writestring(const char *str)
{
    if (str == NULL)
        return;
    user_write(2, str, user_strlen(str)); // Write to stderr (serial)
}

static void user_terminal_clear(void)
{
    // Simple clear screen using ANSI codes
    user_terminal_writestring("\033[2J\033[H");
}

static int user_get_key_input(void)
{
    // Read keyboard input from user mode
    return user_read(0);
}

// Menu display function
static void user_menu_draw(const Menu *menu)
{
    if (menu == NULL)
        return;

    user_terminal_clear();

    // Draw title bar
    const char *title = menu->title;
    size_t title_len = user_strlen(title);
    size_t total_width = 80;
    size_t padding = (total_width - title_len - 4) / 2;

    // Draw title
    for (size_t i = 0; i < padding; i++)
        user_terminal_putchar('=');
    user_terminal_writestring("=== ");
    user_terminal_writestring(title);
    user_terminal_writestring(" ===");
    for (size_t i = 0; i < (total_width - (padding + 4 + title_len + 4)); i++)
        user_terminal_putchar('=');
    user_terminal_putchar('\n');
    user_terminal_putchar('\n');

    // Draw menu items
    for (size_t i = 0; i < menu->item_count; i++)
    {
        const MenuItem *item = &menu->items[i];

        if (i == menu->current_selection)
        {
            user_terminal_writestring(" > ");
        }
        else
        {
            user_terminal_writestring("   ");
        }

        user_terminal_writestring(item->label);
        user_terminal_putchar('\n');
    }

    // Status bar
    user_terminal_putchar('\n');
    user_terminal_writestring("================================================================================\n");
    user_terminal_writestring("Press ENTER to select, UP/DOWN to navigate, ESC to exit\n");
}

// Menu input handler
static Menu *user_menu_handle_input(Menu *menu)
{
    if (menu == NULL)
        return NULL;

    int key = user_get_key_input();

    if (key < 0)
    {
        return menu; // No input
    }

    // Key handling
    if (key == 72 || key == 'w' || key == 'W') // UP or W
    {
        if (menu->current_selection > 0)
            menu->current_selection--;
    }
    else if (key == 80 || key == 's' || key == 'S') // DOWN or S
    {
        if (menu->current_selection < menu->item_count - 1)
            menu->current_selection++;
    }
    else if (key == 13 || key == '\n' || key == '\r') // ENTER
    {
        MenuItem *item = &menu->items[menu->current_selection];
        if (item->submenu != NULL)
        {
            item->submenu->current_selection = 0;
            return item->submenu;
        }

        if (item->callback != NULL)
        {
            item->callback();
        }
        return menu;
    }
    else if (key == 27) // ESC
    {
        if (menu->parent != NULL)
        {
            return menu->parent;
        }
        return NULL; // Exit the root menu
    }

    return menu;
}

// Menu callbacks (simplified)
static void menu_boot_os(void)
{
    user_terminal_writestring("\n\nBooting fishboneOS...\n");
    user_terminal_writestring("Menu system exiting.\n");
    user_sleep(1000); // Sleep 1 second
    // Exit the menu task
    user_exit(0);
}

static void menu_shutdown(void)
{
    user_terminal_writestring("\n\nShutting down fishboneOS...\n");
    user_sleep(1000); // Sleep 1 second
    // Exit the menu task
    user_exit(0);
}

static void menu_memory_test(void)
{
    user_terminal_writestring("\n\nMemory Test - Not implemented in user mode\n");
    user_terminal_writestring("Press any key to return to menu...\n");
    user_get_key_input();
}

static void menu_cpu_details(void)
{
    user_terminal_writestring("\n\nCPU Details - Not implemented in user mode\n");
    user_terminal_writestring("Press any key to return to menu...\n");
    user_get_key_input();
}

static void menu_system_info(void)
{
    user_terminal_writestring("\n\nSystem Information - Not implemented in user mode\n");
    user_terminal_writestring("Press any key to return to menu...\n");
    user_get_key_input();
}

// Menu structure definitions
static MenuItem main_menu_items[] = {
    {"Boot OS", menu_boot_os, NULL, true},
    {"System Information", menu_system_info, NULL, true},
    {"Memory Test", menu_memory_test, NULL, true},
    {"Shutdown", menu_shutdown, NULL, true}};

static Menu main_menu = {
    .title = "Main Menu",
    .items = main_menu_items,
    .item_count = 4,
    .current_selection = 0,
    .parent = NULL};

// Main entry point for user-mode menu task
void user_main(void)
{
    // Initialize user-mode data segments
    user_init_data_segments();

    // Simple menu
    char menu_str[] = "fishboneOS Menu\n";
    user_write(1, menu_str, sizeof(menu_str) - 1);
    char boot_str[] = "1. Boot OS\n";
    user_write(1, boot_str, sizeof(boot_str) - 1);
    char shutdown_str[] = "2. Shutdown\n";
    user_write(1, shutdown_str, sizeof(shutdown_str) - 1);
    char select_str[] = "Select option: ";
    user_write(2, select_str, sizeof(select_str) - 1);

    while (1)
    {
        int key = user_read(0);
        if (key == '1')
        {
            char booting_str[] = "\nBooting...\n";
            user_write(1, booting_str, sizeof(booting_str) - 1);
            user_exit(0);
        }
        else if (key == '2')
        {
            char shutting_str[] = "\nShutting down...\n";
            user_write(1, shutting_str, sizeof(shutting_str) - 1);
            user_exit(0);
        }
        // Yield to allow other tasks to run
        user_yield();
    }
}
