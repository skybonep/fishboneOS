#ifndef KERNEL_SYSCALL_H
#define KERNEL_SYSCALL_H

#include <stdint.h>
#include <kernel/task.h>

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
#define SYS_LOAD_MODULE 14
#define SYS_UNLOAD_MODULE 15

task_context_t *syscall_dispatch(uint32_t interrupt, uint32_t *saved_regs);
int sys_write(int fd, const char *buf, uint32_t len);
void sys_exit(int status);
int sys_fork(void);
int sys_exec(const char *path, const char *const argv[], const char *const envp[]);
int sys_wait(int pid, int *status);
void *sys_alloc(uint32_t size);
int sys_sleep(uint32_t ms);
int sys_read(int fd);
int sys_yield(void);
int sys_open(const char *path);
int sys_read_file(int fd, uint8_t *buffer, uint32_t count);
int sys_write_file(int fd, const uint8_t *buffer, uint32_t count);
int sys_close(int fd);
int sys_load_module(const void *elf_data, size_t elf_size);
int sys_unload_module(const char *name);

#endif /* KERNEL_SYSCALL_H */
