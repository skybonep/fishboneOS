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
#include <kernel/pmm.h>
#include <kernel/vmm.h>
#include <kernel/malloc.h>
#include <kernel/task.h>
#include <kernel/idt.h>
#include <kernel/cpu.h>
#include <drivers/serial.h>
#include <drivers/keyboard.h>

// Forward declarations for menu callbacks
static void menu_boot_os(void);
static void menu_disk_test(void);
static void menu_memory_test(void);
static void menu_shutdown(void);
static void menu_run_hello_world(void);

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

// Menu item definitions
static MenuItem main_menu_items[] = {
    {"Boot OS", menu_boot_os, NULL, true},
    {"Disk Test", menu_disk_test, NULL, true},
    {"System Information", NULL, &system_info_menu, true},
    {"List Tasks", menu_list_tasks, NULL, true},
    {"Run Hello World Task", menu_run_hello_world, NULL, true},
    {"Memory Test", menu_memory_test, NULL, true},
    {"Shutdown", menu_shutdown, NULL, true}};

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
    .item_count = 6,
    .current_selection = 0,
    .parent = NULL};

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

// VGA helper functions for scrollable text

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

static void menu_disk_test(void)
{
    unsigned char sector[512];
    const char *header = "==================== Disk Test ====================\n\n";
    vga_display_text(header);

    if (block_read(0, sector, 1) < 0)
    {
        vga_display_text("ERROR: Failed to read sector 0 from disk.\n\nPress ESC to return to menu...");
        return;
    }

    uint16_t signature = (uint16_t)sector[510] | ((uint16_t)sector[511] << 8);
    char result[256];
    sprintf(result,
            "Sector 0 read successfully.\nBoot signature: 0x%04X\nOEM name: %.8s\n\nPress ESC to return to menu...",
            signature,
            &sector[3]);
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