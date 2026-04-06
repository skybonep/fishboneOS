#define OS_NAME "fishboneOS"
#define ADDRESS_SPACE_END 0xFFFFFFFF

#include <stdlib.h>
#include <string.h>

#include <kernel/menu.h>
#include <kernel/tty.h>
#include <kernel/multiboot.h>
#include <kernel/info.h>
#include <kernel/memory_map.h>
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
static void menu_memory_test(void);
static void menu_shutdown(void);

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
static void display_scrollable_text(const char *title, const char *const lines[], size_t line_count);

// Utility function to wait for key press
static void wait_for_keypress(void);
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
    {"System Information", NULL, &system_info_menu, true},
    {"List Tasks", menu_list_tasks, NULL, true},
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
    .item_count = 5,
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
    terminal_writestring("Booting " OS_NAME "...\n");
    terminal_writestring("Menu system exiting, starting user tasks...\n");
    // This will be handled by kernel integration
}

static void menu_memory_test(void)
{
    terminal_init();
    terminal_writestring("==================== Memory Test =====================\n\n");

    simple_heap_test();

    terminal_writestring("\nPress ESC to return to menu...");
}

static void menu_shutdown(void)
{
    terminal_init();
    terminal_writestring("==================== System Shutdown ====================\n\n");
    terminal_writestring("Shutting down " OS_NAME "...\n");
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

// Info display functions
static void display_cpu_info(void)
{
    terminal_init();
    terminal_writestring("==================== CPU Details =====================\n\n");

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

    unsigned int eax = 0;
    unsigned int ebx = 0;
    unsigned int ecx = 0;
    unsigned int edx = 0;
    bool has_cpuid = cpu_has_cpuid();
    unsigned int stepping = 0;
    unsigned int model = 0;
    unsigned int family = 0;
    unsigned int ext_model = 0;
    unsigned int ext_family = 0;
    unsigned int display_family = 0;
    unsigned int display_model = 0;

    if (has_cpuid)
    {
        cpu_cpuid(1, &eax, &ebx, &ecx, &edx);
        stepping = eax & 0xF;
        model = (eax >> 4) & 0xF;
        family = (eax >> 8) & 0xF;
        ext_model = (eax >> 16) & 0xF;
        ext_family = (eax >> 20) & 0xFF;
        display_family = family;
        display_model = model;
        if (family == 0x6 || family == 0xF)
        {
            display_model |= (ext_model << 4);
        }
        if (family == 0xF)
        {
            display_family += ext_family;
        }
    }

    char buf[16];
    terminal_writestring("CPU Information:\n");
    terminal_writestring("- Vendor: ");
    terminal_writestring(vendor);
    terminal_writestring("\n");

    terminal_writestring("- Family: ");
    itoa(display_family, buf, 10);
    terminal_writestring(buf);
    terminal_writestring("\n");

    terminal_writestring("- Model: ");
    itoa(display_model, buf, 10);
    terminal_writestring(buf);
    terminal_writestring("\n");

    terminal_writestring("- Stepping: ");
    itoa(stepping, buf, 10);
    terminal_writestring(buf);
    terminal_writestring("\n");

    terminal_writestring("\nFeatures:\n");
    if (has_cpuid)
    {
        bool any = false;
        if (edx & (1 << 3))
        {
            terminal_writestring("- PSE (Page Size Extension)\n");
            any = true;
        }
        if (edx & (1 << 6))
        {
            terminal_writestring("- PAE (Physical Address Extension)\n");
            any = true;
        }
        if (edx & (1 << 5))
        {
            terminal_writestring("- MSR (Model-Specific Registers)\n");
            any = true;
        }
        if (edx & (1 << 7))
        {
            terminal_writestring("- MCE (Machine Check Exception)\n");
            any = true;
        }
        if (edx & (1 << 8))
        {
            terminal_writestring("- CX8 (CMPXCHG8B instruction)\n");
            any = true;
        }
        if (edx & (1 << 9))
        {
            terminal_writestring("- APIC (Advanced Programmable Interrupt Controller)\n");
            any = true;
        }
        if (edx & (1 << 13))
        {
            terminal_writestring("- PGE (Page Global Enable)\n");
            any = true;
        }
        if (edx & (1 << 23))
        {
            terminal_writestring("- MMX (MultiMedia eXtensions)\n");
            any = true;
        }
        if (edx & (1 << 25))
        {
            terminal_writestring("- SSE (Streaming SIMD Extensions)\n");
            any = true;
        }
        if (edx & (1 << 26))
        {
            terminal_writestring("- SSE2 (Streaming SIMD Extensions 2)\n");
            any = true;
        }
        if (!any)
        {
            terminal_writestring("- No standard CPUID features detected\n");
        }
    }
    else
    {
        terminal_writestring("- CPUID not supported\n");
    }

    unsigned int total_kb = pmm_get_total_memory_kb();
    unsigned int free_kb = pmm_get_free_memory_kb();
    unsigned int kernel_reserved_kb = ((uint32_t)&kernel_physical_end - (uint32_t)&kernel_physical_start) / 1024;

    terminal_writestring("\nMemory Summary:\n");
    terminal_writestring("- Total RAM: ");
    itoa(total_kb / 1024, buf, 10);
    terminal_writestring(buf);
    terminal_writestring(" MB\n");

    terminal_writestring("- Available RAM: ");
    itoa(free_kb / 1024, buf, 10);
    terminal_writestring(buf);
    terminal_writestring(" MB\n");

    terminal_writestring("- Kernel reserved: ");
    itoa(kernel_reserved_kb / 1024, buf, 10);
    terminal_writestring(buf);
    terminal_writestring(" MB\n");

    terminal_writestring("\nPress ESC to return...");
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

    display_scrollable_text("==================== Memory Information ====================", lines, sizeof(lines) / sizeof(lines[0]));
}

static void display_interrupt_info(void)
{
    terminal_init();
    terminal_writestring("==================== Interrupt Table ====================\n\n");

    char buf[16];
    terminal_writestring("IDT Information:\n");
    terminal_writestring("- Total entries: 256\n");
    terminal_writestring("- Used entries: ");
    unsigned int used_entries = idt_get_used_entries();
    itoa(used_entries, buf, 10);
    terminal_writestring(buf);
    terminal_writestring("\n");

    terminal_writestring("\nConfigured Interrupt Handlers:\n");

    // Get list of configured interrupts
    unsigned int configured_interrupts[256];
    unsigned int configured_count = 0;
    idt_get_configured_interrupts(configured_interrupts, 256, &configured_count);

    for (unsigned int i = 0; i < configured_count; i++)
    {
        unsigned int int_num = configured_interrupts[i];
        terminal_writestring("- INT ");
        itoa(int_num, buf, 10);
        terminal_writestring(buf);
        terminal_writestring(": ");

        // Describe the interrupt based on its number
        if (int_num <= 31)
        {
            terminal_writestring("CPU Exception");
        }
        else if (int_num == 32)
        {
            terminal_writestring("Timer (PIT)");
        }
        else if (int_num == 33)
        {
            terminal_writestring("Keyboard (PS/2)");
        }
        else if (int_num == 128)
        {
            terminal_writestring("System Call");
        }
        else if (int_num >= 34 && int_num <= 47)
        {
            terminal_writestring("Hardware IRQ ");
            itoa(int_num - 32, buf, 10);
            terminal_writestring(buf);
        }
        else
        {
            terminal_writestring("Unknown");
        }
        terminal_writestring("\n");
    }

    terminal_writestring("\nPIC Configuration:\n");
    terminal_writestring("- Master PIC: IRQs 0-7 -> INTs 32-39\n");
    terminal_writestring("- Slave PIC: IRQs 8-15 -> INTs 40-47\n");

    terminal_writestring("\nPress ESC to return...");
}

static void display_device_info(void)
{
    terminal_init();
    terminal_writestring("==================== Device List ====================\n\n");

    terminal_writestring("Detected Devices:\n");

    // Check serial port
    if (serial_is_faulty(SERIAL_COM1_BASE) == 0)
    {
        terminal_writestring("- Serial Port (COM1) - Detected\n");
    }
    else
    {
        terminal_writestring("- Serial Port (COM1) - Not detected\n");
    }

    terminal_writestring("- Timer (PIT 8253/8254) - Detected\n");
    terminal_writestring("- Keyboard (PS/2) - Detected\n");
    terminal_writestring("- VGA Text Mode Display - Detected\n");

    terminal_writestring("\nI/O Ports:\n");

    char buf[16];
    terminal_writestring("- ");
    format_hex_padded(buf, 0x20);
    terminal_writestring(buf);
    terminal_writestring("-");
    format_hex_padded(buf, 0x21);
    terminal_writestring(buf);
    terminal_writestring(": Master PIC\n");

    terminal_writestring("- ");
    format_hex_padded(buf, 0xA0);
    terminal_writestring(buf);
    terminal_writestring("-");
    format_hex_padded(buf, 0xA1);
    terminal_writestring(buf);
    terminal_writestring(": Slave PIC\n");

    terminal_writestring("- ");
    format_hex_padded(buf, 0x40);
    terminal_writestring(buf);
    terminal_writestring("-");
    format_hex_padded(buf, 0x43);
    terminal_writestring(buf);
    terminal_writestring(": PIT Timer\n");

    terminal_writestring("- ");
    format_hex_padded(buf, 0x60);
    terminal_writestring(buf);
    terminal_writestring(": PS/2 Keyboard Data\n");

    terminal_writestring("- ");
    format_hex_padded(buf, 0x64);
    terminal_writestring(buf);
    terminal_writestring(": PS/2 Keyboard Status\n");

    terminal_writestring("- ");
    format_hex_padded(buf, SERIAL_COM1_BASE);
    terminal_writestring(buf);
    terminal_writestring("-");
    format_hex_padded(buf, SERIAL_COM1_BASE + 7);
    terminal_writestring(buf);
    terminal_writestring(": Serial COM1\n");

    terminal_writestring("- ");
    format_hex_padded(buf, 0xB8000);
    terminal_writestring(buf);
    terminal_writestring(": VGA Text Buffer\n");

    terminal_writestring("\nPress ESC to return...\n");
}

static void wait_for_keypress(void);

static void wait_for_keypress(void)
{
    while (!keyboard_has_event())
    {
        // Busy wait for keypress
    }
    keyboard_get_event(); // Consume the key
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

static void display_scrollable_text(const char *title, const char *const lines[], size_t line_count)
{
    const size_t footer_lines = 2;
    const size_t title_lines = 1;
    const size_t viewport_lines = 25 - footer_lines - title_lines;
    size_t offset = 0;

    while (1)
    {
        terminal_init();

        // Render pinned title at the top.
        terminal_setcursor(0, 0);
        terminal_writestring(title);
        terminal_putchar('\n');

        size_t printed = 0;
        for (size_t i = offset; i < line_count && printed < viewport_lines; i++, printed++)
        {
            terminal_writestring(lines[i]);
            terminal_putchar('\n');
        }

        for (; printed < viewport_lines; printed++)
        {
            terminal_putchar('\n');
        }

        terminal_setcursor(0, 23);
        terminal_writestring("================================================================================");
        terminal_setcursor(0, 24);
        terminal_writestring("UP/DOWN to scroll, ENTER/ESC to return");

        while (!keyboard_has_event())
        {
            // Busy wait for navigation key
        }

        char key = keyboard_get_event();
        if (key == KEY_UP || key == 'k' || key == 'K')
        {
            if (offset > 0)
            {
                offset--;
            }
        }
        else if (key == KEY_DOWN || key == 'j' || key == 'J')
        {
            if (offset + viewport_lines < line_count)
            {
                offset++;
            }
        }
        else if (key == KEY_ESC || key == KEY_ENTER || key == '\n' || key == '\r')
        {
            break;
        }
    }
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
    terminal_init();
    terminal_writestring("==================== TASK LIST ====================\n\n");
    terminal_writestring("PID  | Type   | State    | Quantum | Ticks\n");
    terminal_writestring("-----|--------|----------|---------|-------\n");

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
        char pid_buf[8], quantum_buf[8], ticks_buf[8];
        itoa(task->pid, pid_buf, 10);
        itoa(task->quantum, quantum_buf, 10);
        itoa(task->ticks, ticks_buf, 10);

        terminal_writestring(" ");
        terminal_writestring(pid_buf);
        terminal_writestring("   | ");
        terminal_writestring(type_str);
        terminal_writestring(" | ");
        terminal_writestring(state_str);
        terminal_writestring(" | ");
        terminal_writestring(quantum_buf);
        terminal_writestring("      | ");
        terminal_writestring(ticks_buf);
        terminal_writestring("\n");
    }

    terminal_writestring("\nPress ESC to return...\n");
}

// Public interface
Menu *get_main_menu(void)
{
    terminal_writestring("get_main_menu() called\n");
    return &main_menu;
}