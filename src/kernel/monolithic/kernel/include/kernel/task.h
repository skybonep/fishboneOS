#ifndef KERNEL_TASK_H
#define KERNEL_TASK_H

#include <stdint.h>

#define TASK_MAX 16
#define TASK_QUANTUM_DEFAULT 5

typedef enum task_state
{
    TASK_UNUSED = 0,
    TASK_READY,
    TASK_RUNNING,
    TASK_WAITING,
    TASK_ZOMBIE
} task_state_t;

typedef struct task_context
{
    uint32_t edi;
    uint32_t esi;
    uint32_t ebp;
    uint32_t esp;
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
    uint32_t eax;
    uint32_t eip;
    uint32_t cs;
    uint32_t eflags;
} task_context_t;

typedef struct task
{
    uint32_t pid;
    task_state_t state;
    uint32_t quantum;
    uint32_t ticks;

    uint32_t *stack_base;
    uint32_t *stack_top;
    uint32_t stack_size;

    task_context_t context;

    struct task *next;
    struct task *prev;
} task_t;

void task_init(void);
task_t *task_create(void (*entry_point)(void));
task_context_t *task_schedule(void);
task_context_t *task_tick(void);
task_t *task_get_current(void);
void task_set_current(task_t *task);
void task_save_current_context(void *cpu_state_ptr);

#endif /* KERNEL_TASK_H */
