#ifndef KERNEL_USER_SYSCALLS_H
#define KERNEL_USER_SYSCALLS_H

#include <stdint.h>
#include <stddef.h>
#include <kernel/syscall.h>
#include <kernel/gdt.h>

/**
 * User-mode syscall wrapper functions
 * These are inline functions that make int 0x80 syscalls
 */

static inline void user_init_data_segments(void)
{
    asm volatile(
        "movw %0, %%ax\n\t"
        "movw %%ax, %%ds\n\t"
        "movw %%ax, %%es\n\t"
        "movw %%ax, %%fs\n\t"
        "movw %%ax, %%gs\n\t"
        :
        : "i"(USER_DATA_SEG)
        : "ax");
}

static inline int user_write(int fd, const char *buf, uint32_t len)
{
    register uint32_t eax asm("eax") = SYS_WRITE;
    register uint32_t ebx asm("ebx") = (uint32_t)fd;
    register const char *ecx asm("ecx") = buf;
    register uint32_t edx asm("edx") = len;

    asm volatile("int $0x80"
                 : "+a"(eax)
                 : "b"(ebx), "c"(ecx), "d"(edx)
                 : "memory");

    return (int)eax;
}

static inline int user_read(int fd)
{
    register uint32_t eax asm("eax") = SYS_READ;
    register uint32_t ebx asm("ebx") = (uint32_t)fd;

    asm volatile("int $0x80"
                 : "+a"(eax)
                 : "b"(ebx)
                 : "memory");

    return (int)eax;
}

static inline int user_open(const char *path)
{
    register uint32_t eax asm("eax") = SYS_OPEN;
    register const char *ebx asm("ebx") = path;

    asm volatile("int $0x80"
                 : "+a"(eax)
                 : "b"(ebx)
                 : "memory");

    return (int)eax;
}

static inline int user_read_file(int fd, void *buf, uint32_t len)
{
    register uint32_t eax asm("eax") = SYS_READ_FILE;
    register uint32_t ebx asm("ebx") = (uint32_t)fd;
    register void *ecx asm("ecx") = buf;
    register uint32_t edx asm("edx") = len;

    asm volatile("int $0x80"
                 : "+a"(eax)
                 : "b"(ebx), "c"(ecx), "d"(edx)
                 : "memory");

    return (int)eax;
}

static inline int user_write_file(int fd, const void *buf, uint32_t len)
{
    register uint32_t eax asm("eax") = SYS_WRITE_FILE;
    register uint32_t ebx asm("ebx") = (uint32_t)fd;
    register const void *ecx asm("ecx") = buf;
    register uint32_t edx asm("edx") = len;

    asm volatile("int $0x80"
                 : "+a"(eax)
                 : "b"(ebx), "c"(ecx), "d"(edx)
                 : "memory");

    return (int)eax;
}

static inline int user_close(int fd)
{
    register uint32_t eax asm("eax") = SYS_CLOSE;
    register uint32_t ebx asm("ebx") = (uint32_t)fd;

    asm volatile("int $0x80"
                 : "+a"(eax)
                 : "b"(ebx)
                 : "memory");

    return (int)eax;
}

static inline void user_exit(int status)
{
    register uint32_t eax asm("eax") = SYS_EXIT;
    register uint32_t ebx asm("ebx") = (uint32_t)status;

    asm volatile("int $0x80"
                 : "+a"(eax)
                 : "b"(ebx)
                 : "memory");

    // Should not return
    for (;;)
    {
        asm volatile("hlt");
    }
}

static inline int user_fork(void)
{
    register uint32_t eax asm("eax") = SYS_FORK;

    asm volatile("int $0x80"
                 : "+a"(eax)
                 :
                 : "memory");

    return (int)eax;
}

static inline int user_exec(const char *path, const char *const argv[], const char *const envp[])
{
    register uint32_t eax asm("eax") = SYS_EXEC;
    register const char *ebx asm("ebx") = path;
    register const char *const *ecx asm("ecx") = argv;
    register const char *const *edx asm("edx") = envp;

    asm volatile("int $0x80"
                 : "+a"(eax)
                 : "b"(ebx), "c"(ecx), "d"(edx)
                 : "memory");

    return (int)eax;
}

static inline int user_wait(int pid, int *status)
{
    register uint32_t eax asm("eax") = SYS_WAIT;
    register int ebx asm("ebx") = pid;
    register int *ecx asm("ecx") = status;

    asm volatile("int $0x80"
                 : "+a"(eax)
                 : "b"(ebx), "c"(ecx)
                 : "memory");

    return (int)eax;
}

static inline void *user_alloc(uint32_t size)
{
    register uint32_t eax asm("eax") = SYS_ALLOC;
    register uint32_t ebx asm("ebx") = size;

    asm volatile("int $0x80"
                 : "+a"(eax)
                 : "b"(ebx)
                 : "memory");

    return (void *)(uintptr_t)eax;
}

static inline int user_sleep(uint32_t ms)
{
    register uint32_t eax asm("eax") = SYS_SLEEP;
    register uint32_t ebx asm("ebx") = ms;

    asm volatile("int $0x80"
                 : "+a"(eax)
                 : "b"(ebx)
                 : "memory");

    return (int)eax;
}

static inline int user_yield(void)
{
    register uint32_t eax asm("eax") = SYS_YIELD;

    asm volatile("int $0x80"
                 : "+a"(eax)
                 :
                 : "memory");

    return (int)eax;
}

#endif /* KERNEL_USER_SYSCALLS_H */
