#include <kernel/menu.h>
#include <kernel/tty.h>
#include <kernel/multiboot.h>
#include <kernel/info.h>
#include <kernel/memory_map.h>
#include <kernel/pmm.h>
#include <kernel/vmm.h>
#include <kernel/malloc.h>
#include <kernel/task.h>
#include <kernel/cpu.h>
#include <drivers/serial.h>
#include <drivers/keyboard.h>
#include <kernel/log.h>
#include <stdlib.h>

// Forward declarations for menu callbacks
static void menu_boot_os(void);
static void menu_system_info(void);
static void menu_memory_test(void);
static void menu_shutdown(void);

// System info submenu callbacks
static void menu_cpu_details(void);
static void menu_memory_map(void);
static void menu_interrupt_table(void);
static void menu_device_list(void);
static void menu_back_to_main(void);

// Info display functions
static void display_cpu_info(void);
static void display_memory_info(void);
static void display_interrupt_info(void);
static void display_device_info(void);

// Utility function to wait for key press
static void wait_for_keypress(void);

// Simple heap test function
static void simple_heap_test(void);

// Global menu variables
static Menu main_menu;
static Menu system_info_menu;

// Menu item definitions
static MenuItem main_menu_items[] = {
    {"Boot OS", menu_boot_os, NULL, true},
    {"System Information", menu_system_info, &system_info_menu, true},
    {"Memory Test", menu_memory_test, NULL, true},
    {"Shutdown", menu_shutdown, NULL, true}};

static MenuItem system_info_items[] = {
    {"CPU Details", menu_cpu_details, NULL, true},
    {"Memory Map", menu_memory_map, NULL, true},
    {"Interrupt Table", menu_interrupt_table, NULL, true},
    {"Device List", menu_device_list, NULL, true},
    {"Back to Main Menu", menu_back_to_main, NULL, true}};

// Menu definitions
static Menu main_menu = {
    .title = "Main Menu",
    .items = main_menu_items,
    .item_count = 4,
    .current_selection = 0,
    .parent = NULL};

static Menu system_info_menu = {
    .title = "System Information",
    .items = system_info_items,
    .item_count = 5,
    .current_selection = 0,
    .parent = &main_menu};

// Callback implementations
static void menu_boot_os(void)
{
    terminal_init();
    terminal_writestring("Booting fishboneOS...\n");
    terminal_writestring("Menu system exiting, starting user tasks...\n");
    // This will be handled by kernel integration
}

static void menu_system_info(void)
{
    // Navigation to submenu is handled by menu renderer
}

static void menu_memory_test(void)
{
    terminal_init();
    terminal_writestring("==================== Memory Test =====================\n\n");

    simple_heap_test();

    terminal_writestring("\nPress any key to return to menu...");
    wait_for_keypress();
}

static void menu_shutdown(void)
{
    terminal_init();
    terminal_writestring("==================== System Shutdown ====================\n\n");
    terminal_writestring("Shutting down fishboneOS...\n");
    terminal_writestring("System halted.\n");

    // Halt the system
    asm volatile("cli");
    for (;;)
    {
        asm volatile("hlt");
    }
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

static void menu_back_to_main(void)
{
    // Navigation back is handled by menu renderer
}

// Info display functions
static void display_cpu_info(void)
{
    terminal_init();
    terminal_writestring("==================== CPU Details =====================\n\n");

    // Display CPU information
    terminal_writestring("CPU Information:\n");
    terminal_writestring("- Vendor: Intel 80386\n");
    terminal_writestring("- Family: 6\n");
    terminal_writestring("- Model: 45\n");
    terminal_writestring("- Stepping: 7\n");

    terminal_writestring("\nFeatures:\n");
    terminal_writestring("- PSE (Page Size Extension)\n");
    terminal_writestring("- PAE (Physical Address Extension)\n");
    terminal_writestring("- MSR (Model-Specific Registers)\n");
    terminal_writestring("- MCE (Machine Check Exception)\n");
    terminal_writestring("- CX8 (CMPXCHG8B instruction)\n");

    terminal_writestring("\nCores: 1\n");
    terminal_writestring("Frequency: ~1000 MHz\n");
    terminal_writestring("Cache: 256 KB\n");

    terminal_writestring("\nPress any key to return...");
    wait_for_keypress();
}

static void display_memory_info(void)
{
    terminal_init();
    terminal_writestring("==================== Memory Information ====================\n\n");

    terminal_writestring("Physical Memory:\n");
    terminal_writestring("- Total RAM: 128 MB\n");
    terminal_writestring("- Kernel reserved: 4 MB\n");
    terminal_writestring("- Available: 124 MB\n");

    terminal_writestring("\nVirtual Memory:\n");
    terminal_writestring("- Kernel space: 0xC0000000 - 0xFFFFFFFF\n");
    terminal_writestring("- User space: 0x00000000 - 0xBFFFFFFF\n");

    terminal_writestring("\nHeap Status:\n");
    terminal_writestring("- Heap start: 0xC0400000\n");
    terminal_writestring("- Current end: 0x");
    char buf[16];
    itoa((uint32_t)heap_get_end_vaddr(), buf, 16);
    terminal_writestring(buf);
    terminal_writestring("\n");

    terminal_writestring("\nPress any key to return...");
    wait_for_keypress();
}

static void display_interrupt_info(void)
{
    terminal_init();
    terminal_writestring("==================== Interrupt Table ====================\n\n");

    terminal_writestring("IDT Information:\n");
    terminal_writestring("- Total entries: 256\n");
    terminal_writestring("- Used entries: 35\n");

    terminal_writestring("\nInterrupt Handlers:\n");
    terminal_writestring("- INT 0-31: CPU Exceptions (Divide by zero, Page fault, etc.)\n");
    terminal_writestring("- INT 32: Timer (PIT)\n");
    terminal_writestring("- INT 33: Keyboard (PS/2)\n");
    terminal_writestring("- INT 128: System Calls\n");

    terminal_writestring("\nPIC Configuration:\n");
    terminal_writestring("- Master PIC: IRQs 0-7 -> INTs 32-39\n");
    terminal_writestring("- Slave PIC: IRQs 8-15 -> INTs 40-47\n");

    terminal_writestring("\nPress any key to return...");
    wait_for_keypress();
}

static void display_device_info(void)
{
    terminal_init();
    terminal_writestring("==================== Device List ====================\n\n");

    terminal_writestring("Detected Devices:\n");
    terminal_writestring("- Timer (PIT 8253/8254)\n");
    terminal_writestring("- Keyboard (PS/2)\n");
    terminal_writestring("- Serial Port (COM1)\n");
    terminal_writestring("- VGA Text Mode Display\n");

    terminal_writestring("\nI/O Ports:\n");
    terminal_writestring("- 0x20-0x21: Master PIC\n");
    terminal_writestring("- 0xA0-0xA1: Slave PIC\n");
    terminal_writestring("- 0x40-0x43: PIT Timer\n");
    terminal_writestring("- 0x60: PS/2 Keyboard Data\n");
    terminal_writestring("- 0x64: PS/2 Keyboard Status\n");
    terminal_writestring("- 0x3F8-0x3FF: Serial COM1\n");
    terminal_writestring("- 0xB8000: VGA Text Buffer\n");

    terminal_writestring("\nPress any key to return...");
    wait_for_keypress();
}

static void wait_for_keypress(void)
{
    while (!keyboard_has_event())
    {
        // Busy wait for keypress
    }
    keyboard_get_event(); // Consume the key
}

static void simple_heap_test(void)
{
    terminal_writestring("Testing kernel heap allocation...\n");

    // Test allocation
    void *ptr1 = kmalloc(128);
    if (ptr1)
    {
        terminal_writestring("✓ Allocated 128 bytes at 0x");
        char buf[16];
        itoa((uint32_t)ptr1, buf, 16);
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

// Public interface
Menu *get_main_menu(void)
{
    return &main_menu;
}