#ifndef USER_SYSCALLS_H
#define USER_SYSCALLS_H

#include <stdint.h>
#include <stddef.h>

/* Type definitions */
typedef int pid_t;

/* Syscall numbers (must match kernel/include/kernel/syscall.h) */
#define SYS_WRITE 1
#define SYS_EXIT 2
#define SYS_FORK 3
#define SYS_EXEC 4
#define SYS_WAIT 5
#define SYS_ALLOC 6
#define SYS_SLEEP 7
#define SYS_READ 8
#define SYS_YIELD 9
#define SYS_OPEN 10
#define SYS_READ_FILE 11
#define SYS_CLOSE 12
#define SYS_WRITE_FILE 13

/**
 * User-space syscall wrapper functions
 * These are inline functions that make int 0x80 syscalls
 */

static inline int write(int fd, const char *buf, uint32_t len)
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

static inline int read(int fd)
{
    register uint32_t eax asm("eax") = SYS_READ;
    register uint32_t ebx asm("ebx") = (uint32_t)fd;

    asm volatile("int $0x80"
                 : "+a"(eax)
                 : "b"(ebx)
                 : "memory");

    return (int)eax;
}

static inline int open(const char *path)
{
    register uint32_t eax asm("eax") = SYS_OPEN;
    register const char *ebx asm("ebx") = path;

    asm volatile("int $0x80"
                 : "+a"(eax)
                 : "b"(ebx)
                 : "memory");

    return (int)eax;
}

static inline int read_file(int fd, void *buf, uint32_t len)
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

static inline int write_file(int fd, const void *buf, uint32_t len)
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

static inline int close(int fd)
{
    register uint32_t eax asm("eax") = SYS_CLOSE;
    register uint32_t ebx asm("ebx") = (uint32_t)fd;

    asm volatile("int $0x80"
                 : "+a"(eax)
                 : "b"(ebx)
                 : "memory");

    return (int)eax;
}

static inline void exit(int status)
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

static inline int fork(void)
{
    register uint32_t eax asm("eax") = SYS_FORK;

    asm volatile("int $0x80"
                 : "+a"(eax)
                 :
                 : "memory");

    return (int)eax;
}

static inline int exec(const char *path, const char *const argv[], const char *const envp[])
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

static inline int wait(int pid, int *status)
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

static inline void *alloc(uint32_t size)
{
    register uint32_t eax asm("eax") = SYS_ALLOC;
    register uint32_t ebx asm("ebx") = size;

    asm volatile("int $0x80"
                 : "+a"(eax)
                 : "b"(ebx)
                 : "memory");

    return (void *)(uintptr_t)eax;
}

static inline int sleep(uint32_t ms)
{
    register uint32_t eax asm("eax") = SYS_SLEEP;
    register uint32_t ebx asm("ebx") = ms;

    asm volatile("int $0x80"
                 : "+a"(eax)
                 : "b"(ebx)
                 : "memory");

    return (int)eax;
}

static inline int yield(void)
{
    register uint32_t eax asm("eax") = SYS_YIELD;

    asm volatile("int $0x80"
                 : "+a"(eax)
                 :
                 : "memory");

    return (int)eax;
}

#endif /* USER_SYSCALLS_H */