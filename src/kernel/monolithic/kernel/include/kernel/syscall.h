#ifndef KERNEL_SYSCALL_H
#define KERNEL_SYSCALL_H

#include <stdint.h>
#include <kernel/task.h>

#define SYS_WRITE 1
#define SYS_EXIT 2
#define SYS_ALLOC 3
#define SYS_SLEEP 4
#define SYS_READ 5
#define SYS_YIELD 6

task_context_t *syscall_dispatch(uint32_t interrupt, uint32_t *saved_regs);
int sys_write(int fd, const char *buf, uint32_t len);
void sys_exit(int status);
void *sys_alloc(uint32_t size);
int sys_sleep(uint32_t ms);
int sys_read(int fd);
int sys_yield(void);

#endif /* KERNEL_SYSCALL_H */
