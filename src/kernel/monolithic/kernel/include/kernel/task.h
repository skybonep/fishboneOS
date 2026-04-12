#ifndef KERNEL_TASK_H
#define KERNEL_TASK_H

#include <stdint.h>

#define TASK_MAX 16
#define TASK_QUANTUM_DEFAULT 5
#define MAX_USER_CODE_PAGES 16

typedef enum task_state
{
    TASK_UNUSED = 0,
    TASK_READY,
    TASK_RUNNING,
    TASK_WAITING,
    TASK_ZOMBIE
} task_state_t;

typedef enum task_type
{
    TASK_TYPE_KERNEL = 0,
    TASK_TYPE_USER
} task_type_t;

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
    uint32_t ss;
    uint32_t eflags;
} task_context_t;

typedef struct task
{
    uint32_t pid;
    task_state_t state;
    task_type_t type;
    uint32_t quantum;
    uint32_t ticks;

    uint32_t *stack_base;
    uint32_t *stack_top;
    uint32_t stack_size;

    uint32_t *user_stack_top;
    uint32_t user_stack_size;
    uint32_t user_stack_paddr;
    uint32_t user_code_paddr;
    uint32_t user_code_frame_count;
    uint32_t user_code_frames[MAX_USER_CODE_PAGES];

    /* Per-task address space (CR3) */
    uint32_t page_directory_phys;

    uint32_t wake_tick;
    int32_t exit_status;

    task_context_t context;

    struct task *next;
    struct task *prev;
} task_t;

void task_init(void);
task_t *task_create(void (*entry_point)(void));
task_t *task_create_user(void (*entry_point)(void), uint32_t *user_stack_top, uint32_t user_stack_size);
task_t *task_create_user_from_elf(const void *elf_data, size_t elf_size, uint32_t *user_stack_top, uint32_t user_stack_size);
task_context_t *task_schedule(void);
task_context_t *task_tick(void);
task_context_t *task_yield(void);
task_t *task_get_current(void);
void task_set_current(task_t *task);
void task_save_current_context(void *cpu_state_ptr);
void task_exit(int status);
void task_terminate(int status);
task_t *task_get_at_index(uint32_t index);

#endif /* KERNEL_TASK_H */
