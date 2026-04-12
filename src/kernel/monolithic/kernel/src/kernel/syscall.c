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
    default:
        syscall_set_return_value(saved_regs, (uint32_t)-1);
        return NULL;
    }
}
