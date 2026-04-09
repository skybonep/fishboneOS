#include <stdint.h>
#include <stddef.h>

#include <kernel/user_syscalls.h>

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
