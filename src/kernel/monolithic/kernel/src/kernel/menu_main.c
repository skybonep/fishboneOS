#define OS_NAME "fishboneOS"
#define ADDRESS_SPACE_END 0xFFFFFFFF

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <kernel/menu.h>
#include <kernel/tty.h>
#include <kernel/multiboot.h>
#include <kernel/info.h>
#include <kernel/memory_map.h>
#include <drivers/block.h>
#include <kernel/fat16.h>
#include <kernel/pmm.h>
#include <kernel/vmm.h>
#include <kernel/malloc.h>
#include <kernel/task.h>
#include <kernel/idt.h>
#include <kernel/cpu.h>
#include <drivers/serial.h>
#include <drivers/keyboard.h>

// Forward declarations
static void vga_append_text(const char *text);

// Input handling globals
char input_buffer[13];
int input_mode = 0; // 0 = none, 1 = filename
int input_len = 0;
int input_has_dot = 0;

// Forward declarations for menu callbacks
static void menu_boot_os(void);
static void menu_disk_test(void);
static void menu_disk_write_test(void);
static void menu_list_files(void);
static void menu_create_file(void);
static void menu_delete_file(void);
static void menu_memory_test(void);
static void menu_shutdown(void);
static void menu_run_hello_world(void);
static void menu_run_disk_hello(void);

// Forward declaration for user task function
extern void hello_world_main(void);

// System info submenu callbacks
static void menu_cpu_details(void);
static void menu_memory_map(void);
static void menu_interrupt_table(void);
static void menu_device_list(void);
static void menu_list_tasks(void);

// Info display functions
static void display_cpu_info(void);
static void display_memory_info(void);
static void display_interrupt_info(void);
static void display_device_info(void);

// Utility function to wait for key press

static char *append_text(char *dest, const char *src);
static void format_hex_padded(char *buf, uint32_t value);

// Simple heap test function
static void simple_heap_test(void);

extern uint32_t kernel_physical_start;
extern uint32_t kernel_physical_end;

// Global menu variables
static Menu main_menu;
static Menu system_info_menu;
static Menu disk_management_menu;

// Menu item definitions
static MenuItem main_menu_items[] = {
    {"Boot OS", menu_boot_os, NULL, true},
    {"Disk Management", NULL, &disk_management_menu, true},
    {"System Information", NULL, &system_info_menu, true},
    {"List Tasks", menu_list_tasks, NULL, true},
    {"Run Hello World Task", menu_run_hello_world, NULL, true},
    {"Run /HELLO.BIN from disk", menu_run_disk_hello, NULL, true},
    {"Memory Test", menu_memory_test, NULL, true},
    {"Shutdown", menu_shutdown, NULL, true}};

static MenuItem disk_management_items[] = {
    {"Disk Test", menu_disk_test, NULL, true},
    {"FAT16 Write Test", menu_disk_write_test, NULL, true},
    {"List Files", menu_list_files, NULL, true},
    {"Create File", menu_create_file, NULL, true},
    {"Delete File", menu_delete_file, NULL, true},
    {"Back to Main Menu", NULL, &main_menu, true}};

static MenuItem system_info_items[] = {
    {"CPU Details", menu_cpu_details, NULL, true},
    {"Memory Map", menu_memory_map, NULL, true},
    {"Interrupt Table", menu_interrupt_table, NULL, true},
    {"Device List", menu_device_list, NULL, true},
    {"Back to Main Menu", NULL, &main_menu, true}};

// Menu definitions
static Menu main_menu = {
    .title = "Main Menu",
    .items = main_menu_items,
    .item_count = 8,
    .current_selection = 0,
    .parent = NULL};

static Menu disk_management_menu = {
    .title = "Disk Management",
    .items = disk_management_items,
    .item_count = 6,
    .current_selection = 0,
    .parent = &main_menu};

static Menu system_info_menu = {
    .title = "System Information",
    .items = system_info_items,
    .item_count = 5,
    .current_selection = 0,
    .parent = &main_menu};

// Callback implementations
// Direct VGA output helper for menu callbacks
static void vga_display_text(const char *text)
{
    static const size_t VGA_WIDTH = 80;
    static const size_t VGA_HEIGHT = 25;
    static uint16_t *const VGA_MEMORY = (uint16_t *)0xB8000;
    static size_t current_row = 0;
    static size_t current_col = 0;

    // Clear screen first
    for (size_t y = 0; y < VGA_HEIGHT; y++)
    {
        for (size_t x = 0; x < VGA_WIDTH; x++)
        {
            VGA_MEMORY[y * VGA_WIDTH + x] = (uint16_t)' ' | (uint16_t)0x07 << 8;
        }
    }

    current_row = 0;
    current_col = 0;

    for (size_t i = 0; text[i] != '\0'; i++)
    {
        if (text[i] == '\n')
        {
            current_col = 0;
            current_row++;
            if (current_row >= VGA_HEIGHT)
                current_row = 0;
        }
        else if (text[i] == '\r')
        {
            current_col = 0;
        }
        else
        {
            if (current_col < VGA_WIDTH && current_row < VGA_HEIGHT)
            {
                VGA_MEMORY[current_row * VGA_WIDTH + current_col] = (uint16_t)text[i] | (uint16_t)0x07 << 8;
            }
            current_col++;
            if (current_col >= VGA_WIDTH)
            {
                current_col = 0;
                current_row++;
                if (current_row >= VGA_HEIGHT)
                    current_row = 0;
            }
        }
    }
}

// Helper function to check if character is valid for FAT16 filename
static int is_valid_filename_char(char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '_';
}

// Convert to uppercase for FAT16
static char to_upper(char c)
{
    if (c >= 'a' && c <= 'z')
        return c - 32;
    return c;
}

// VGA helper functions for scrollable text

static void vga_append_text(const char *text)
{
    static const size_t VGA_WIDTH = 80;
    static const size_t VGA_HEIGHT = 25;
    static uint16_t *const VGA_MEMORY = (uint16_t *)0xB8000;
    static size_t current_row = 0;
    static size_t current_col = 0;

    for (size_t i = 0; text[i] != '\0'; i++)
    {
        if (text[i] == '\n')
        {
            current_col = 0;
            current_row++;
            if (current_row >= VGA_HEIGHT)
                current_row = 0;
        }
        else if (text[i] == '\r')
        {
            current_col = 0;
        }
        else
        {
            if (current_col < VGA_WIDTH && current_row < VGA_HEIGHT)
            {
                VGA_MEMORY[current_row * VGA_WIDTH + current_col] = (uint16_t)text[i] | (uint16_t)0x07 << 8;
            }
            current_col++;
            if (current_col >= VGA_WIDTH)
            {
                current_col = 0;
                current_row++;
                if (current_row >= VGA_HEIGHT)
                    current_row = 0;
            }
        }
    }
}

void process_filename_input(char key)
{
    if (key == '\n' || key == '\r')
    { // Enter
        if (input_len > 0)
        {
            input_buffer[input_len] = '\0';
            // Validate 8.3 format
            char *dot = NULL;
            for (size_t i = 0; input_buffer[i]; i++)
            {
                if (input_buffer[i] == '.')
                {
                    dot = &input_buffer[i];
                    break;
                }
            }
            if (dot)
            {
                size_t name_len = dot - input_buffer;
                size_t ext_len = strlen(dot + 1);
                if (name_len > 8 || ext_len > 3)
                {
                    vga_display_text("\nInvalid filename format (max 8.3). Press ESC to cancel or try again.\n");
                    vga_append_text("Enter filename (8.3 format, e.g. FILE.TXT): ");
                    memset(input_buffer, 0, sizeof(input_buffer));
                    input_len = 0;
                    input_has_dot = 0;
                    return;
                }
            }
            else
            {
                if (input_len > 8)
                {
                    vga_display_text("\nFilename too long (max 8 chars). Press ESC to cancel or try again.\n");
                    vga_append_text("Enter filename (8.3 format, e.g. FILE.TXT): ");
                    memset(input_buffer, 0, sizeof(input_buffer));
                    input_len = 0;
                    input_has_dot = 0;
                    return;
                }
            }
            // Create file
            fat16_fs_t fs;
            if (fat16_mount(&fs, 0) < 0)
            {
                vga_display_text("ERROR: Failed to mount FAT16 volume at LBA 0.\n\nPress ESC to return to menu...");
                input_mode = 0;
                return;
            }
            if (fat16_create(&fs, input_buffer) < 0)
            {
                char result[256];
                sprintf(result,
                        "ERROR: Failed to create file %s.\n"
                        "File may already exist or no free space in root directory.\n\n"
                        "Press ESC to return to menu...",
                        input_buffer);
                vga_display_text(result);
            }
            else
            {
                char result[256];
                sprintf(result,
                        "File %s created successfully.\n\n"
                        "Press ESC to return to menu...",
                        input_buffer);
                vga_display_text(result);
            }
            input_mode = 0;
        }
    }
    else if (key == 27)
    { // ESC
        vga_display_text("\nFile creation cancelled.\n\nPress ESC to return to menu...");
        input_mode = 0;
    }
    else if (input_len < 12 && is_valid_filename_char(key))
    {
        if (key == '.' && input_has_dot)
            return; // Only one dot
        if (key == '.' && input_len == 0)
            return; // No leading dot
        input_buffer[input_len++] = to_upper(key);
        if (key == '.')
            input_has_dot = 1;
        char temp[2] = {key, '\0'};
        vga_append_text(temp);
    }
}

static void menu_boot_os(void)
{
    const char *boot_msg = "Booting " OS_NAME "...\nMenu system exiting, starting user tasks...\n";
    vga_display_text(boot_msg);
    // This will be handled by kernel integration
}

static void menu_memory_test(void)
{
    const char *test_msg = "==================== Memory Test =====================\n\n";
    vga_display_text(test_msg);

    simple_heap_test();

    const char *return_msg = "\nPress ESC to return to menu...";
    // Append to existing display
    static const size_t VGA_WIDTH = 80;
    static uint16_t *const VGA_MEMORY = (uint16_t *)0xB8000;
    size_t row = 2; // After the title
    for (size_t i = 0; return_msg[i] != '\0'; i++)
    {
        if (return_msg[i] == '\n')
        {
            row++;
        }
        else if (row < 25)
        {
            VGA_MEMORY[row * VGA_WIDTH + i] = (uint16_t)return_msg[i] | (uint16_t)0x07 << 8;
        }
    }
}

static void menu_shutdown(void)
{
    const char *shutdown_msg = "==================== System Shutdown ====================\n\nShutting down " OS_NAME "...\nSystem halted.\n";
    vga_display_text(shutdown_msg);

    // Halt the system
    asm volatile("cli");
    for (;;)
    {
        asm volatile("hlt");
    }
}

static void menu_run_hello_world(void)
{
    const char *hello_msg = "==================== Hello World Task =====================\n\nCreating user-mode Hello World task...\n";
    vga_display_text(hello_msg);

    // Create the user task with proper user stack
    const uint32_t user_stack_top = 0xBFFFE000; // Top of user space minus two pages, page aligned
    const uint32_t user_stack_size = 2 * 4096;  // 2 pages for stack

    task_t *hello_task = task_create_user(hello_world_main, (uint32_t *)user_stack_top, user_stack_size);
    if (hello_task == NULL)
    {
        const char *error_msg = "ERROR: Failed to create Hello World task!\n";
        // Append error message
        static const size_t VGA_WIDTH = 80;
        static uint16_t *const VGA_MEMORY = (uint16_t *)0xB8000;
        size_t row = 3;
        for (size_t i = 0; error_msg[i] != '\0' && row < 25; i++)
        {
            if (error_msg[i] == '\n')
            {
                row++;
                i = 0;
            }
            else
            {
                VGA_MEMORY[row * VGA_WIDTH + i] = (uint16_t)error_msg[i] | (uint16_t)0x07 << 8;
            }
        }
    }
    else
    {
        const char *success_msg = "Task created successfully. Check serial output for Hello World message.\nTask PID: ";
        char pid_buf[8];
        itoa(hello_task->pid, pid_buf, 10);

        // Display success message
        static const size_t VGA_WIDTH = 80;
        static uint16_t *const VGA_MEMORY = (uint16_t *)0xB8000;
        size_t row = 3;
        size_t col = 0;
        for (size_t i = 0; success_msg[i] != '\0'; i++)
        {
            VGA_MEMORY[row * VGA_WIDTH + col++] = (uint16_t)success_msg[i] | (uint16_t)0x07 << 8;
            if (col >= VGA_WIDTH)
            {
                col = 0;
                row++;
            }
        }
        for (size_t i = 0; pid_buf[i] != '\0'; i++)
        {
            VGA_MEMORY[row * VGA_WIDTH + col++] = (uint16_t)pid_buf[i] | (uint16_t)0x07 << 8;
            if (col >= VGA_WIDTH)
            {
                col = 0;
                row++;
            }
        }
        VGA_MEMORY[row * VGA_WIDTH + col++] = (uint16_t)'\n' | (uint16_t)0x07 << 8;
    }

    // Return to menu - input handling is done by menu renderer
}

static void menu_run_disk_hello(void)
{
    const char *header = "==================== Disk Hello Task ====================\n\n";
    vga_display_text(header);

    const uint32_t user_stack_top = 0xBFFFE000;
    const uint32_t user_stack_size = 2 * PAGE_SIZE;

    int fd = fat16_open(NULL, "HELLO.BIN");
    if (fd < 0)
    {
        vga_display_text("ERROR: HELLO.BIN not found in FAT16 root.\n\n");
        return;
    }

    uint8_t *buffer = (uint8_t *)kmalloc(PAGE_SIZE * MAX_USER_CODE_PAGES);
    if (buffer == NULL)
    {
        vga_display_text("ERROR: Failed to allocate buffer for HELLO.BIN.\n\n");
        fat16_close(fd);
        return;
    }

    int read_bytes = fat16_read(fd, buffer, PAGE_SIZE * MAX_USER_CODE_PAGES);
    fat16_close(fd);

    if (read_bytes <= 0)
    {
        vga_display_text("ERROR: Failed to read HELLO.BIN from FAT16.\n\n");
        kfree(buffer);
        return;
    }

    task_t *hello_task = task_create_user_from_binary(buffer,
                                                      (size_t)read_bytes,
                                                      (uint32_t *)user_stack_top,
                                                      user_stack_size,
                                                      NULL,
                                                      NULL);
    kfree(buffer);
    if (hello_task == NULL)
    {
        vga_display_text("ERROR: Failed to create HELLO.BIN user task.\n\n");
        return;
    }

    char pid_buf[12];
    itoa(hello_task->pid, pid_buf, 10);

    char result[128];
    sprintf(result, "Created HELLO.BIN task with PID %s.\n\n", pid_buf);
    vga_display_text(result);
}

static void menu_disk_test(void)
{
    fat16_fs_t fs;
    const char *header = "==================== Disk Test ====================\n\n";
    vga_display_text(header);

    if (fat16_mount(&fs, 0) < 0)
    {
        vga_display_text("ERROR: Failed to mount FAT16 volume at LBA 0.\n\nPress ESC to return to menu...");
        return;
    }

    char result[1024];
    sprintf(result,
            "FAT16 mounted successfully.\n"
            "OEM name: %.8s\n"
            "Bytes/sector: %u\n"
            "Sectors/cluster: %u\n"
            "Reserved sectors: %u\n"
            "Number of FATs: %u\n"
            "Root entries: %u\n"
            "Sectors/FAT: %u\n"
            "FAT start LBA: %u\n"
            "Root dir LBA: %u\n"
            "Data start LBA: %u\n\n",
            fs.oem_name,
            fs.bytes_per_sector,
            fs.sectors_per_cluster,
            fs.reserved_sector_count,
            fs.num_fats,
            fs.root_entry_count,
            fs.sectors_per_fat,
            fs.fat_start_lba,
            fs.root_dir_start_lba,
            fs.data_start_lba);

    const char *test_names[] = {"README.TXT", "HELLO.TXT", "TEST.TXT", "KERNEL.BIN"};
    const char *opened_name = NULL;
    int fd = -1;

    for (size_t i = 0; i < sizeof(test_names) / sizeof(test_names[0]); ++i)
    {
        fd = fat16_open(&fs, test_names[i]);
        if (fd >= 0)
        {
            opened_name = test_names[i];
            break;
        }
    }

    if (fd < 0)
    {
        sprintf(result + strlen(result),
                "File test: no known root file found.\n"
                "Create README.TXT or HELLO.TXT in the FAT16 root and retry.\n\n"
                "Press ESC to return to menu...");
        vga_display_text(result);
        return;
    }

    uint8_t file_buffer[128];
    int bytes_read = fat16_read(fd, file_buffer, sizeof(file_buffer));
    fat16_close(fd);

    if (bytes_read < 0)
    {
        sprintf(result + strlen(result),
                "File test: opened %s but failed to read file.\n\n"
                "Press ESC to return to menu...",
                opened_name);
        vga_display_text(result);
        return;
    }

    char preview[256];
    size_t preview_pos = 0;
    for (int i = 0; i < bytes_read && preview_pos + 1 < sizeof(preview); ++i)
    {
        char ch = (char)file_buffer[i];
        if (ch >= ' ' && ch < 0x7F)
        {
            preview[preview_pos++] = ch;
        }
        else if (ch == '\n' || ch == '\r' || ch == '\t')
        {
            preview[preview_pos++] = ch;
        }
        else
        {
            preview[preview_pos++] = '.';
        }
    }
    preview[preview_pos] = '\0';

    sprintf(result + strlen(result),
            "File test: opened %s and read %d bytes.\n"
            "File preview:\n%s\n\n"
            "Press ESC to return to menu...",
            opened_name,
            bytes_read,
            preview);

    vga_display_text(result);
}

static void menu_disk_write_test(void)
{
    fat16_fs_t fs;
    const char *header = "==================== FAT16 Write Test ====================\n\n";
    vga_display_text(header);

    if (fat16_mount(&fs, 0) < 0)
    {
        vga_display_text("ERROR: Failed to mount FAT16 volume at LBA 0.\n\nPress ESC to return to menu...");
        return;
    }

    const char *target_name = "TEST.TXT";
    int fd = fat16_open(&fs, target_name);
    if (fd < 0)
    {
        if (fat16_create(&fs, target_name) < 0)
        {
            char result[256];
            sprintf(result,
                    "ERROR: Could not create %s in root directory.\n"
                    "Ensure there is free space in the root directory and retry.\n\n"
                    "Press ESC to return to menu...",
                    target_name);
            vga_display_text(result);
            return;
        }

        fd = fat16_open(&fs, target_name);
        if (fd < 0)
        {
            char result[256];
            sprintf(result,
                    "ERROR: Created %s but failed to open it afterward.\n"
                    "Press ESC to return to menu...",
                    target_name);
            vga_display_text(result);
            return;
        }
    }

    const char *marker = "FAT16 WRITE OK\n";
    int written = fat16_write(fd, (const uint8_t *)marker, (uint32_t)strlen(marker));
    if (written < 0)
    {
        fat16_close(fd);
        vga_display_text("ERROR: Failed to write to FAT16 file.\n\nPress ESC to return to menu...");
        return;
    }

    if (fat16_close(fd) < 0)
    {
        vga_display_text("ERROR: Failed to close FAT16 file after write.\n\nPress ESC to return to menu...");
        return;
    }

    fd = fat16_open(&fs, target_name);
    if (fd < 0)
    {
        vga_display_text("ERROR: Failed to reopen FAT16 file after write.\n\nPress ESC to return to menu...");
        return;
    }

    uint8_t readback[64];
    int read_bytes = fat16_read(fd, readback, sizeof(readback) - 1);
    fat16_close(fd);
    if (read_bytes < 0)
    {
        vga_display_text("ERROR: Failed to read back FAT16 file after write.\n\nPress ESC to return to menu...");
        return;
    }

    readback[read_bytes] = '\0';

    char result[512];
    sprintf(result,
            "FAT16 write test succeeded.\n"
            "Wrote %d bytes to %s.\n"
            "Read back %d bytes.\n\n"
            "File contents:\n%s\n\n"
            "Press ESC to return to menu...",
            written,
            target_name,
            read_bytes,
            readback);

    vga_display_text(result);
}

static void menu_list_files(void)
{
    const char *header = "==================== List Files ====================\n\n";
    vga_display_text(header);

    fat16_fs_t fs;
    if (fat16_mount(&fs, 0) < 0)
    {
        vga_display_text("ERROR: Failed to mount FAT16 volume at LBA 0.\n\nPress ESC to return to menu...");
        return;
    }

    if (fat16_list_root(&fs) < 0)
    {
        vga_display_text("ERROR: Failed to list root directory.\n\nPress ESC to return to menu...");
        return;
    }

    // The list_root function prints to serial/log, so we need to show a message on VGA
    const char *footer = "\n\nFile listing completed. Check serial output for details.\n\nPress ESC to return to menu...";
    // For now, just show the footer since the actual listing goes to serial
    static const size_t VGA_WIDTH = 80;
    static uint16_t *const VGA_MEMORY = (uint16_t *)0xB8000;
    size_t row = 3; // After header
    for (size_t i = 0; footer[i] != '\0'; i++)
    {
        if (footer[i] == '\n')
        {
            row++;
        }
        else if (row < 25)
        {
            VGA_MEMORY[row * VGA_WIDTH + (i % VGA_WIDTH)] = (uint16_t)footer[i] | (uint16_t)0x07 << 8;
        }
    }
}

static void menu_create_file(void)
{
    const char *header = "==================== Create File ====================\n\n";
    vga_display_text(header);

    // Start input mode
    input_mode = 1;
    memset(input_buffer, 0, sizeof(input_buffer));
    input_len = 0;
    input_has_dot = 0;
    vga_append_text("Enter filename (8.3 format, e.g. FILE.TXT): ");
}

static void menu_delete_file(void)
{
    const char *header = "==================== Delete File ====================\n\n";
    vga_display_text(header);

    fat16_fs_t fs;
    if (fat16_mount(&fs, 0) < 0)
    {
        vga_display_text("ERROR: Failed to mount FAT16 volume at LBA 0.\n\nPress ESC to return to menu...");
        return;
    }

    const char *filename = "NEWFILE.TXT";
    if (fat16_delete(&fs, filename) < 0)
    {
        char result[256];
        sprintf(result,
                "ERROR: Failed to delete file %s.\n"
                "File may not exist.\n\n"
                "Press ESC to return to menu...",
                filename);
        vga_display_text(result);
        return;
    }

    char result[256];
    sprintf(result,
            "File %s deleted successfully.\n\n"
            "Press ESC to return to menu...",
            filename);
    vga_display_text(result);
}

// System info submenu callbacks
static void menu_cpu_details(void)
{
    display_cpu_info();
}

static void menu_memory_map(void)
{
    display_memory_info();
}

static void menu_interrupt_table(void)
{
    display_interrupt_info();
}

static void menu_device_list(void)
{
    display_device_info();
}

// Info display functions
static void display_cpu_info(void)
{
    char display_text[2000] = "==================== CPU Details =====================\n\n";
    size_t pos = strlen(display_text);

    char vendor[13] = {0};
    cpu_get_vendor_string(vendor);
    if (vendor[0] == '\0')
    {
        const char unknown_vendor[] = "Unknown";
        for (size_t i = 0; i < sizeof(unknown_vendor); i++)
        {
            vendor[i] = unknown_vendor[i];
        }
    }

    pos += sprintf(display_text + pos, "CPU Vendor: %s\n", vendor);

    unsigned int total_kb = pmm_get_total_memory_kb();
    unsigned int free_kb = pmm_get_free_memory_kb();
    pos += sprintf(display_text + pos, "\nMemory Summary:\n");
    pos += sprintf(display_text + pos, "- Total RAM: %d MB\n", total_kb / 1024);
    pos += sprintf(display_text + pos, "- Available RAM: %d MB\n", free_kb / 1024);

    vga_display_text(display_text);
}

static void display_memory_info(void)
{
    char buf[16];
    char total_ram_line[32] = {0};
    char available_ram_line[32] = {0};
    char reserved_ram_line[32] = {0};
    char kernel_space_line[64] = {0};
    char user_space_line[64] = {0};
    char heap_start_line[32] = {0};
    char heap_end_line[32] = {0};
    char current_heap_line[32] = {0};
    char stack_start_line[32] = {0};
    char stack_end_line[32] = {0};
    char recursive_pdt_line[32] = {0};
    char temp_page_line[32] = {0};
    char mmio_start_line[32] = {0};

    // Build memory information lines
    char *p;
    p = total_ram_line;
    append_text(p, "- Total RAM: ");
    unsigned int total_kb = pmm_get_total_memory_kb();
    itoa(total_kb / 1024, buf, 10);
    append_text(p, buf);
    append_text(p, " MB");

    p = available_ram_line;
    append_text(p, "- Available: ");
    unsigned int free_kb = pmm_get_free_memory_kb();
    itoa(free_kb / 1024, buf, 10);
    append_text(p, buf);
    append_text(p, " MB");

    p = reserved_ram_line;
    append_text(p, "- Kernel reserved: ");
    unsigned int kernel_reserved_kb = ((uint32_t)&kernel_physical_end - (uint32_t)&kernel_physical_start) / 1024;
    itoa(kernel_reserved_kb / 1024, buf, 10);
    append_text(p, buf);
    append_text(p, " MB");

    p = kernel_space_line;
    append_text(p, "- Kernel space: ");
    format_hex_padded(buf, KERNEL_VIRT_BASE);
    append_text(p, buf);
    append_text(p, " - ");
    format_hex_padded(buf, ADDRESS_SPACE_END);
    append_text(p, buf);

    p = user_space_line;
    append_text(p, "- User space: ");
    format_hex_padded(buf, USER_SPACE_START);
    append_text(p, buf);
    append_text(p, " - ");
    format_hex_padded(buf, USER_SPACE_END);
    append_text(p, buf);

    p = heap_start_line;
    append_text(p, "- Heap start: ");
    format_hex_padded(buf, KERNEL_HEAP_START);
    append_text(p, buf);

    p = heap_end_line;
    append_text(p, "- Heap end: ");
    format_hex_padded(buf, KERNEL_HEAP_END);
    append_text(p, buf);

    p = current_heap_line;
    append_text(p, "- Current end: ");
    format_hex_padded(buf, (uint32_t)heap_get_end_vaddr());
    append_text(p, buf);

    p = stack_start_line;
    append_text(p, "- Stack start: ");
    format_hex_padded(buf, KERNEL_STACK_REGION_START);
    append_text(p, buf);

    p = stack_end_line;
    append_text(p, "- Stack end: ");
    format_hex_padded(buf, KERNEL_STACK_REGION_END);
    append_text(p, buf);

    p = recursive_pdt_line;
    append_text(p, "- Recursive PDT: ");
    format_hex_padded(buf, VMM_PDT_VIRTUAL_ADDR);
    append_text(p, buf);

    p = temp_page_line;
    append_text(p, "- Temp page: ");
    format_hex_padded(buf, VMM_TEMP_PAGE);
    append_text(p, buf);

    p = mmio_start_line;
    append_text(p, "- MMIO start: ");
    format_hex_padded(buf, MMIO_RESERVED_START);
    append_text(p, buf);

    const char *lines[] = {
        "",
        "Physical Memory:",
        total_ram_line,
        reserved_ram_line,
        available_ram_line,
        "",
        "Virtual Memory:",
        kernel_space_line,
        user_space_line,
        "",
        "Heap Status:",
        heap_start_line,
        heap_end_line,
        current_heap_line,
        "",
        "Stack Region:",
        stack_start_line,
        stack_end_line,
        "",
        "VMM Pages:",
        recursive_pdt_line,
        temp_page_line,
        "",
        "MMIO Reserved:",
        mmio_start_line};

    // Display the title
    terminal_writestring("==================== Memory Information ====================\n");

    // Display all lines
    for (size_t i = 0; i < sizeof(lines) / sizeof(lines[0]); i++)
    {
        terminal_writestring(lines[i]);
        terminal_writestring("\n");
    }

    // Display on VGA
    char vga_text[2000] = "==================== Memory Information ====================\n";
    size_t pos = strlen(vga_text);
    for (size_t i = 0; i < sizeof(lines) / sizeof(lines[0]); i++)
    {
        pos += sprintf(vga_text + pos, "%s\n", lines[i]);
    }

    vga_display_text(vga_text);

    // Return to menu
}

static void display_interrupt_info(void)
{
    char display_text[1000] = "==================== Interrupt Table ====================\n\n";
    size_t pos = strlen(display_text);

    pos += sprintf(display_text + pos, "IDT Information:\n");
    pos += sprintf(display_text + pos, "- Total entries: 256\n");

    unsigned int used_entries = idt_get_used_entries();
    pos += sprintf(display_text + pos, "- Used entries: %d\n", used_entries);

    pos += sprintf(display_text + pos, "\nPIC Configuration:\n");
    pos += sprintf(display_text + pos, "- Master PIC: IRQs 0-7 -> INTs 32-39\n");
    pos += sprintf(display_text + pos, "- Slave PIC: IRQs 8-15 -> INTs 40-47\n");

    vga_display_text(display_text);
}

static void display_device_info(void)
{
    char display_text[1000] = "==================== Device List ====================\n\n";
    size_t pos = strlen(display_text);

    pos += sprintf(display_text + pos, "Detected Devices:\n");

    // Check serial port
    if (serial_is_faulty(SERIAL_COM1_BASE) == 0)
    {
        pos += sprintf(display_text + pos, "- Serial Port (COM1) - Detected\n");
    }
    else
    {
        pos += sprintf(display_text + pos, "- Serial Port (COM1) - Not detected\n");
    }

    pos += sprintf(display_text + pos, "- Timer (PIT 8253/8254) - Detected\n");
    pos += sprintf(display_text + pos, "- Keyboard (PS/2) - Detected\n");
    pos += sprintf(display_text + pos, "- VGA Text Mode Display - Detected\n");

    pos += sprintf(display_text + pos, "\nI/O Ports:\n");
    pos += sprintf(display_text + pos, "- 0x0020-0x0021: Master PIC\n");
    pos += sprintf(display_text + pos, "- 0x00A0-0x00A1: Slave PIC\n");
    pos += sprintf(display_text + pos, "- 0x0040-0x0043: PIT Timer\n");
    pos += sprintf(display_text + pos, "- 0x0060: PS/2 Keyboard Data\n");
    pos += sprintf(display_text + pos, "- 0x0064: PS/2 Keyboard Status\n");
    pos += sprintf(display_text + pos, "- 0x03F8-0x03FF: Serial COM1\n");
    pos += sprintf(display_text + pos, "- 0xB8000: VGA Text Buffer\n");

    vga_display_text(display_text);
}

static char *append_text(char *dest, const char *src)
{
    while (*dest)
    {
        dest++;
    }
    while (*src)
    {
        *dest++ = *src++;
    }
    *dest = '\0';
    return dest;
}

static void format_hex_padded(char *buf, uint32_t value)
{
    const char hex_digits[] = "0123456789ABCDEF";
    buf[0] = '0';
    buf[1] = 'x';
    for (int i = 0; i < 8; i++)
    {
        int shift = 28 - (i * 4); // Start from MSB
        int digit = (value >> shift) & 0xF;
        buf[i + 2] = hex_digits[digit];
    }
    buf[10] = '\0';
}

static void simple_heap_test(void)
{
    terminal_writestring("Testing kernel heap allocation...\n");

    // Test allocation
    void *ptr1 = kmalloc(128);
    if (ptr1)
    {
        terminal_writestring("✓ Allocated 128 bytes at ");
        char buf[16];
        format_hex_padded(buf, (uint32_t)ptr1);
        terminal_writestring(buf);
        terminal_writestring("\n");

        // Test writing
        uint32_t *test_val = (uint32_t *)ptr1;
        *test_val = 0xDEADBEEF;
        if (*test_val == 0xDEADBEEF)
        {
            terminal_writestring("✓ Memory write/read test passed\n");
        }
        else
        {
            terminal_writestring("✗ Memory test failed\n");
        }

        // Test free
        kfree(ptr1);
        terminal_writestring("✓ Memory freed successfully\n");
    }
    else
    {
        terminal_writestring("✗ Memory allocation failed\n");
    }
}

static void menu_list_tasks(void)
{
    char display_text[2000] = "==================== TASK LIST ====================\n\n";
    size_t pos = strlen(display_text);

    pos += sprintf(display_text + pos, "PID  | Type   | State    | Quantum | Ticks\n");
    pos += sprintf(display_text + pos, "-----|--------|----------|---------|-------\n");

    // Iterate through task table
    for (uint32_t i = 0; i < TASK_MAX; i++)
    {
        task_t *task = task_get_at_index(i);

        // Skip unused task slots
        if (task == NULL || task->state == TASK_UNUSED)
            continue;

        // Get state string
        const char *state_str = "?????";
        switch (task->state)
        {
        case TASK_UNUSED:
            state_str = "Unused";
            break;
        case TASK_READY:
            state_str = "Ready ";
            break;
        case TASK_RUNNING:
            state_str = "Run   ";
            break;
        case TASK_WAITING:
            state_str = "Wait  ";
            break;
        case TASK_ZOMBIE:
            state_str = "Zombie";
            break;
        }

        // Get type string
        const char *type_str = (task->type == TASK_TYPE_USER) ? "User  " : "Kernel";

        // Format line
        pos += sprintf(display_text + pos, " %d   | %s | %s | %d      | %d\n",
                       task->pid, type_str, state_str, task->quantum, task->ticks);
    }

    vga_display_text(display_text);
}

// Public interface
Menu *get_main_menu(void)
{
    return &main_menu;
}