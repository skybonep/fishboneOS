#include <stddef.h>
#include <stdint.h>

#include <drivers/keyboard.h>
#include <drivers/serial.h>
#include <drivers/timer.h>
#include <kernel/fat16.h>
#include <kernel/malloc.h>
#include <kernel/syscall.h>
#include <kernel/task.h>
#include <kernel/tty.h>
#include <kernel/log.h>
#include <kernel/module.h>
#include <kernel/elf_loader.h>
#include <kernel/vmm.h>
#include <kernel/paging.h>

static uint32_t syscall_get_u32_arg(uint32_t *saved_regs, int arg_index)
{
    switch (arg_index)
    {
    case 0:
        return saved_regs[4]; /* EBX */
    case 1:
        return saved_regs[6]; /* ECX */
    case 2:
        return saved_regs[5]; /* EDX */
    case 3:
        return saved_regs[1]; /* ESI */
    case 4:
        return saved_regs[0]; /* EDI */
    default:
        return 0;
    }
}

static void syscall_set_return_value(uint32_t *saved_regs, uint32_t value)
{
    saved_regs[7] = value; /* EAX */
}

static bool validate_user_pointer(const void *ptr, size_t size)
{
    if (ptr == NULL || size == 0)
        return true; // NULL pointers are valid for some cases, zero size is always valid

    uint32_t start_vaddr = (uint32_t)ptr;
    uint32_t end_vaddr = start_vaddr + size - 1;

    // Check if addresses are in user space
    if (start_vaddr >= KERNEL_VIRT_BASE || end_vaddr >= KERNEL_VIRT_BASE)
        return false;

    // Check if all pages in the range are mapped
    for (uint32_t vaddr = start_vaddr & ~(PAGE_SIZE - 1); vaddr <= end_vaddr; vaddr += PAGE_SIZE)
    {
        if (vmm_get_phys_addr(vaddr) == 0)
            return false;
    }

    return true;
}

static bool validate_user_string(const char *str)
{
    if (str == NULL)
        return true; // NULL strings are valid for some cases

    uint32_t vaddr = (uint32_t)str;

    // Check if starting address is in user space
    if (vaddr >= KERNEL_VIRT_BASE)
        return false;

    // Walk through the string, checking each page
    while (true)
    {
        // Check if current page is mapped
        uint32_t page_start = vaddr & ~(PAGE_SIZE - 1);
        if (vmm_get_phys_addr(page_start) == 0)
            return false;

        // Find null terminator within this page
        uint32_t page_end = page_start + PAGE_SIZE;
        uint32_t max_check = (vaddr + 256 < page_end) ? vaddr + 256 : page_end; // Limit search to reasonable length

        for (; vaddr < max_check; vaddr++)
        {
            // We can't directly dereference user pointers here, but we assume
            // the string is valid if the pages are mapped. In a real implementation,
            // we'd need to temporarily map the page to check for null terminator.
            // For now, we'll just validate that the memory range is accessible.
        }

        // If we haven't found null terminator and reached page boundary,
        // continue to next page (but limit total string length)
        if (vaddr >= max_check)
            return false; // String too long or not null-terminated within reasonable bounds
    }

    return true;
}

static bool validate_user_pointer_array(const void *const *ptrs)
{
    if (ptrs == NULL)
        return true; // NULL arrays are valid for some cases

    uint32_t array_vaddr = (uint32_t)ptrs;

    // Check if array pointer is in user space
    if (array_vaddr >= KERNEL_VIRT_BASE)
        return false;

    // Check if array page is mapped
    uint32_t page_start = array_vaddr & ~(PAGE_SIZE - 1);
    if (vmm_get_phys_addr(page_start) == 0)
        return false;

    // For simplicity, we'll validate the first few pointers in the array
    // In a real implementation, we'd need to walk through all pointers until NULL
    const void *const *current = ptrs;
    for (int i = 0; i < 32; i++) // Limit to reasonable number of arguments
    {
        // Check if current pointer position is still in mapped memory
        uint32_t current_vaddr = (uint32_t)current;
        if (current_vaddr >= KERNEL_VIRT_BASE)
            break;

        uint32_t current_page = current_vaddr & ~(PAGE_SIZE - 1);
        if (vmm_get_phys_addr(current_page) == 0)
            return false;

        // If this pointer is NULL, end of array
        if (*current == NULL)
            break;

        // Validate the pointed-to string/pointer
        if (!validate_user_string((const char *)*current))
            return false;

        current++;
    }

    return true;
}

int sys_write(int fd, const char *buf, uint32_t len)
{
    if (buf == NULL)
    {
        return -1;
    }

    if (fd == 1)
    {
        terminal_write(buf, len);
        return (int)len;
    }

    if (fd == 2)
    {
        for (uint32_t i = 0; i < len; ++i)
        {
            char ch = buf[i];
            char tmp[2] = {ch, '\0'};
            serial_write(SERIAL_COM1_BASE, tmp);
        }
        return (int)len;
    }

    return -1;
}

void sys_exit(int status)
{
    task_terminate(status);
}

void *sys_alloc(uint32_t size)
{
    if (size == 0)
    {
        return NULL;
    }

    return kmalloc((size_t)size);
}

int sys_sleep(uint32_t ms)
{
    if (ms == 0)
    {
        return 0;
    }

    uint32_t ticks = timer_get_ticks();
    uint32_t delay_ticks = (ms + 9u) / 10u;
    task_t *current = task_get_current();
    if (current == NULL)
    {
        return -1;
    }

    current->wake_tick = ticks + (delay_ticks == 0 ? 1 : delay_ticks);
    current->state = TASK_WAITING;
    return 0;
}

int sys_read(int fd)
{
    // Currently only support fd=0 (keyboard input)
    if (fd != 0)
    {
        return -1;
    }

    // Block until a keyboard event is available
    while (!keyboard_has_event())
    {
        // Yield CPU while waiting for input
        task_t *current = task_get_current();
        if (current != NULL)
        {
            current->state = TASK_WAITING;
            task_schedule();
        }
    }

    // Return the keyboard character
    return (int)keyboard_get_event();
}

int sys_open(const char *path)
{
    if (path == NULL)
    {
        return -1;
    }

    return fat16_open(NULL, path);
}

int sys_read_file(int fd, uint8_t *buffer, uint32_t count)
{
    if (buffer == NULL || count == 0)
    {
        return -1;
    }

    return fat16_read(fd, buffer, count);
}

int sys_write_file(int fd, const uint8_t *buffer, uint32_t count)
{
    if (buffer == NULL || count == 0)
    {
        return -1;
    }

    return fat16_write(fd, buffer, count);
}

int sys_close(int fd)
{
    return fat16_close(fd);
}

int sys_yield(void)
{
    // Voluntary yield: enqueue current task and switch to next
    return 0;
}

int sys_load_module(const void *elf_data, size_t elf_size)
{
    if (elf_data == NULL || elf_size == 0)
    {
        return -1;
    }

    return module_load(elf_data, elf_size);
}

int sys_unload_module(const char *name)
{
    if (name == NULL)
    {
        return -1;
    }

    return module_unload(name);
}

int sys_fork(void)
{
    task_t *current = task_get_current();
    if (current == NULL || current->type != TASK_TYPE_USER_PROCESS)
    {
        return -1;
    }

    task_t *child = task_alloc();
    if (child == NULL)
    {
        return -1;
    }

    uint32_t child_pdt = vmm_clone_kernel_mappings();
    if (child_pdt == 0)
    {
        child->state = TASK_UNUSED;
        return -1;
    }

    child->page_directory_phys = child_pdt;

    // Copy user code pages
    child->user_code_frame_count = current->user_code_frame_count;
    for (uint32_t i = 0; i < current->user_code_frame_count; ++i)
    {
        uint32_t new_frame = (uint32_t)pmm_alloc_frame();
        if (new_frame == 0)
        {
            // cleanup
            for (uint32_t j = 0; j < i; ++j)
            {
                pmm_free_frame((void *)(uintptr_t)child->user_code_frames[j]);
            }
            child->state = TASK_UNUSED;
            return -1;
        }

        child->user_code_frames[i] = new_frame;

        // Copy page content
        uint32_t temp_src = VMM_TEMP_PAGE;
        vmm_map_kernel_page(temp_src, current->user_code_frames[i]);
        uint32_t temp_dst = VMM_TEMP_PAGE - PAGE_SIZE;
        vmm_map_kernel_page(temp_dst, new_frame);
        uint8_t *src = (uint8_t *)temp_src;
        uint8_t *dst = (uint8_t *)temp_dst;
        for (uint32_t k = 0; k < PAGE_SIZE; ++k)
        {
            dst[k] = src[k];
        }
        vmm_unmap_page(temp_src);
        vmm_unmap_page(temp_dst);

        // Map in child's PDT
        uint32_t vaddr = current->user_code_paddr + i * PAGE_SIZE;
        vmm_map_page_for_pdt(child_pdt, vaddr, new_frame, PAGE_PRESENT | PAGE_USER);
    }

    // Copy user stack
    uint32_t child_stack_frame = (uint32_t)pmm_alloc_frame();
    if (child_stack_frame == 0)
    {
        // cleanup
        for (uint32_t i = 0; i < child->user_code_frame_count; ++i)
        {
            pmm_free_frame((void *)(uintptr_t)child->user_code_frames[i]);
        }
        child->state = TASK_UNUSED;
        return -1;
    }

    child->user_stack_paddr = child_stack_frame;

    // Copy stack content
    uint32_t temp_src = VMM_TEMP_PAGE;
    vmm_map_kernel_page(temp_src, current->user_stack_paddr);
    uint32_t temp_dst = VMM_TEMP_PAGE - PAGE_SIZE;
    vmm_map_kernel_page(temp_dst, child_stack_frame);
    uint8_t *src = (uint8_t *)temp_src;
    uint8_t *dst = (uint8_t *)temp_dst;
    for (uint32_t k = 0; k < PAGE_SIZE; ++k)
    {
        dst[k] = src[k];
    }
    vmm_unmap_page(temp_src);
    vmm_unmap_page(temp_dst);

    // Map stack
    uint32_t stack_vaddr = (uint32_t)current->user_stack_top - current->user_stack_size + PAGE_SIZE;
    vmm_map_page_for_pdt(child_pdt, stack_vaddr, child_stack_frame, PAGE_PRESENT | PAGE_WRITE | PAGE_USER);

    // Set up child
    child->type = TASK_TYPE_USER_PROCESS;
    child->user_stack_top = current->user_stack_top;
    child->user_stack_size = current->user_stack_size;
    child->user_code_paddr = current->user_code_paddr;
    child->state = TASK_READY;
    child->ticks = 0;
    child->quantum = TASK_QUANTUM_DEFAULT;

    // Parent-child
    child->parent_pid = current->pid;
    child->parent = current;
    task_add_child(current, child);

    // Context
    child->context = current->context;
    child->context.eax = 0; // return 0 to child

    task_enqueue(child);

    return child->pid;
}

int sys_exec(const char *path, const char *const argv[], const char *const envp[])
{
    // Validate user pointers before using them
    if (!validate_user_string(path))
    {
        return -1;
    }

    if (!validate_user_pointer_array((const void *const *)argv))
    {
        return -1;
    }

    if (!validate_user_pointer_array((const void *const *)envp))
    {
        return -1;
    }

    task_t *current = task_get_current();
    if (current == NULL || current->type != TASK_TYPE_USER_PROCESS)
    {
        return -1;
    }

    // Load ELF from file
    int fd = fat16_open(NULL, path);
    if (fd < 0)
    {
        return -1;
    }

    // Get file size - assume small
    uint8_t buffer[PAGE_SIZE * 4]; // max 16KB
    int read = fat16_read(fd, buffer, sizeof(buffer));
    fat16_close(fd);
    if (read <= 0)
    {
        return -1;
    }

    // Replace process image
    if (task_replace_process_image(current, buffer, (size_t)read, argv, envp) != 0)
    {
        return -1;
    }

    return 0;
}

int sys_wait(int pid, int *status)
{
    // Validate user pointer before using it
    if (!validate_user_pointer(status, sizeof(int)))
    {
        return -1;
    }

    task_t *current = task_get_current();
    if (current == NULL || current->type != TASK_TYPE_USER_PROCESS)
    {
        return -1;
    }

    task_t *child = task_find_child(current, pid);
    if (child == NULL)
    {
        return -1; // No such child
    }

    if (child->state == TASK_ZOMBIE)
    {
        // Reap
        *status = child->exit_status;
        int child_pid = child->pid;

        // Clean process resources when reaped
        if (child->user_stack_paddr != 0)
        {
            pmm_free_frame((void *)child->user_stack_paddr);
            child->user_stack_paddr = 0;
        }

        if (child->user_code_paddr != 0)
        {
            pmm_free_frame((void *)child->user_code_paddr);
            child->user_code_paddr = 0;
        }

        for (uint32_t i = 0; i < child->user_code_frame_count; ++i)
        {
            if (child->user_code_frames[i] != 0)
            {
                pmm_free_frame((void *)(uintptr_t)child->user_code_frames[i]);
                child->user_code_frames[i] = 0;
            }
        }
        child->user_code_frame_count = 0;

        task_remove_child(child);
        child->state = TASK_UNUSED;
        return child_pid;
    }
    else
    {
        // Block
        current->waiting_for_pid = pid;
        current->waiting_status = status;
        current->state = TASK_WAITING;
        return 0; // Will be overridden when woken
    }
}

static task_context_t *syscall_handle_write(uint32_t *saved_regs)
{
    int fd = (int)syscall_get_u32_arg(saved_regs, 0);
    const char *buf = (const char *)(uintptr_t)syscall_get_u32_arg(saved_regs, 1);
    uint32_t len = syscall_get_u32_arg(saved_regs, 2);
    printk(LOG_DEBUG, "syscall_write: fd=%d len=%u", fd, len);
    int result = sys_write(fd, buf, len);
    syscall_set_return_value(saved_regs, (uint32_t)result);
    return NULL;
}

static task_context_t *syscall_handle_exit(uint32_t *saved_regs)
{
    int status = (int)syscall_get_u32_arg(saved_regs, 0);
    sys_exit(status);
    return task_schedule();
}

static task_context_t *syscall_handle_alloc(uint32_t *saved_regs)
{
    uint32_t size = syscall_get_u32_arg(saved_regs, 0);
    void *result = sys_alloc(size);
    syscall_set_return_value(saved_regs, (uint32_t)(uintptr_t)result);
    return NULL;
}

static task_context_t *syscall_handle_sleep(uint32_t *saved_regs)
{
    uint32_t ms = syscall_get_u32_arg(saved_regs, 0);
    int result = sys_sleep(ms);
    syscall_set_return_value(saved_regs, (uint32_t)result);
    return task_schedule();
}

static task_context_t *syscall_handle_read(uint32_t *saved_regs)
{
    int fd = (int)syscall_get_u32_arg(saved_regs, 0);
    int result = sys_read(fd);
    syscall_set_return_value(saved_regs, (uint32_t)result);
    return NULL;
}

static task_context_t *syscall_handle_open(uint32_t *saved_regs)
{
    const char *path = (const char *)(uintptr_t)syscall_get_u32_arg(saved_regs, 0);
    int result = sys_open(path);
    syscall_set_return_value(saved_regs, (uint32_t)result);
    return NULL;
}

static task_context_t *syscall_handle_read_file(uint32_t *saved_regs)
{
    int fd = (int)syscall_get_u32_arg(saved_regs, 0);
    uint8_t *buffer = (uint8_t *)(uintptr_t)syscall_get_u32_arg(saved_regs, 1);
    uint32_t count = syscall_get_u32_arg(saved_regs, 2);
    int result = sys_read_file(fd, buffer, count);
    syscall_set_return_value(saved_regs, (uint32_t)result);
    return NULL;
}

static task_context_t *syscall_handle_write_file(uint32_t *saved_regs)
{
    int fd = (int)syscall_get_u32_arg(saved_regs, 0);
    const uint8_t *buffer = (const uint8_t *)(uintptr_t)syscall_get_u32_arg(saved_regs, 1);
    uint32_t count = syscall_get_u32_arg(saved_regs, 2);
    int result = sys_write_file(fd, buffer, count);
    syscall_set_return_value(saved_regs, (uint32_t)result);
    return NULL;
}

static task_context_t *syscall_handle_close(uint32_t *saved_regs)
{
    int fd = (int)syscall_get_u32_arg(saved_regs, 0);
    int result = sys_close(fd);
    syscall_set_return_value(saved_regs, (uint32_t)result);
    return NULL;
}

static task_context_t *syscall_handle_yield(uint32_t *saved_regs)
{
    int result = sys_yield();
    syscall_set_return_value(saved_regs, (uint32_t)result);
    return task_yield();
}

static task_context_t *syscall_handle_load_module(uint32_t *saved_regs)
{
    const void *elf_data = (const void *)(uintptr_t)syscall_get_u32_arg(saved_regs, 0);
    size_t elf_size = (size_t)syscall_get_u32_arg(saved_regs, 1);
    printk(LOG_DEBUG, "syscall_load_module: data=%p size=%u", elf_data, elf_size);
    int result = sys_load_module(elf_data, elf_size);
    syscall_set_return_value(saved_regs, (uint32_t)result);
    return NULL;
}

static task_context_t *syscall_handle_unload_module(uint32_t *saved_regs)
{
    const char *name = (const char *)(uintptr_t)syscall_get_u32_arg(saved_regs, 0);
    printk(LOG_DEBUG, "syscall_unload_module: name=%s", name ? name : "(null)");
    int result = sys_unload_module(name);
    syscall_set_return_value(saved_regs, (uint32_t)result);
    return NULL;
}

static task_context_t *syscall_handle_fork(uint32_t *saved_regs)
{
    int result = sys_fork();
    syscall_set_return_value(saved_regs, (uint32_t)result);
    return NULL;
}

static task_context_t *syscall_handle_exec(uint32_t *saved_regs)
{
    const char *path = (const char *)(uintptr_t)syscall_get_u32_arg(saved_regs, 0);
    const char *const *argv = (const char *const *)(uintptr_t)syscall_get_u32_arg(saved_regs, 1);
    const char *const *envp = (const char *const *)(uintptr_t)syscall_get_u32_arg(saved_regs, 2);
    int result = sys_exec(path, argv, envp);
    syscall_set_return_value(saved_regs, (uint32_t)result);
    return NULL;
}

static task_context_t *syscall_handle_wait(uint32_t *saved_regs)
{
    int pid = (int)syscall_get_u32_arg(saved_regs, 0);
    int *status = (int *)(uintptr_t)syscall_get_u32_arg(saved_regs, 1);
    int result = sys_wait(pid, status);
    syscall_set_return_value(saved_regs, (uint32_t)result);
    return NULL;
}

task_context_t *syscall_dispatch(uint32_t interrupt, uint32_t *saved_regs)
{
    if (interrupt != 128 || saved_regs == NULL)
    {
        return NULL;
    }

    uint32_t call_number = saved_regs[7];
    printk(LOG_DEBUG, "syscall_dispatch: call_number=%u", call_number);
    switch (call_number)
    {
    case SYS_WRITE:
        return syscall_handle_write(saved_regs);
    case SYS_EXIT:
        return syscall_handle_exit(saved_regs);
    case SYS_ALLOC:
        return syscall_handle_alloc(saved_regs);
    case SYS_SLEEP:
        return syscall_handle_sleep(saved_regs);
    case SYS_READ:
        return syscall_handle_read(saved_regs);
    case SYS_OPEN:
        return syscall_handle_open(saved_regs);
    case SYS_READ_FILE:
        return syscall_handle_read_file(saved_regs);
    case SYS_WRITE_FILE:
        return syscall_handle_write_file(saved_regs);
    case SYS_CLOSE:
        return syscall_handle_close(saved_regs);
    case SYS_YIELD:
        return syscall_handle_yield(saved_regs);
    case SYS_LOAD_MODULE:
        return syscall_handle_load_module(saved_regs);
    case SYS_UNLOAD_MODULE:
        return syscall_handle_unload_module(saved_regs);
    case SYS_FORK:
        return syscall_handle_fork(saved_regs);
    case SYS_EXEC:
        return syscall_handle_exec(saved_regs);
    case SYS_WAIT:
        return syscall_handle_wait(saved_regs);
    default:
        syscall_set_return_value(saved_regs, (uint32_t)-1);
        return NULL;
    }
}
