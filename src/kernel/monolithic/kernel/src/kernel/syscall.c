#include <stddef.h>
#include <stdint.h>

#include <drivers/serial.h>
#include <drivers/timer.h>
#include <kernel/malloc.h>
#include <kernel/syscall.h>
#include <kernel/task.h>
#include <kernel/tty.h>
#include <kernel/log.h>

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

static task_context_t *syscall_handle_write(uint32_t *saved_regs)
{
    int fd = (int)syscall_get_u32_arg(saved_regs, 0);
    const char *buf = (const char *)(uintptr_t)syscall_get_u32_arg(saved_regs, 1);
    uint32_t len = syscall_get_u32_arg(saved_regs, 2);
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

task_context_t *syscall_dispatch(uint32_t interrupt, uint32_t *saved_regs)
{
    if (interrupt != 128 || saved_regs == NULL)
    {
        return NULL;
    }

    uint32_t call_number = saved_regs[7];
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
    default:
        syscall_set_return_value(saved_regs, (uint32_t)-1);
        return NULL;
    }
}
