#include <stddef.h>
#include <kernel/task.h>

static task_t task_table[TASK_MAX];
static task_t *current_task = NULL;
static uint32_t next_pid = 1;

static task_t *task_alloc(void)
{
    for (uint32_t index = 0; index < TASK_MAX; ++index)
    {
        if (task_table[index].state == TASK_UNUSED)
        {
            task_t *task = &task_table[index];
            task->pid = next_pid++;
            task->state = TASK_READY;
            task->quantum = TASK_QUANTUM_DEFAULT;
            task->ticks = 0;
            task->stack_base = NULL;
            task->stack_top = NULL;
            task->stack_size = 0;
            task->context = NULL;
            task->next = NULL;
            task->prev = NULL;
            return task;
        }
    }

    return NULL;
}

void task_init(void)
{
    for (uint32_t index = 0; index < TASK_MAX; ++index)
    {
        task_table[index].pid = 0;
        task_table[index].state = TASK_UNUSED;
        task_table[index].quantum = 0;
        task_table[index].ticks = 0;
        task_table[index].stack_base = NULL;
        task_table[index].stack_top = NULL;
        task_table[index].stack_size = 0;
        task_table[index].context = NULL;
        task_table[index].next = NULL;
        task_table[index].prev = NULL;
    }

    current_task = NULL;
    next_pid = 1;
}

task_t *task_create(void (*entry_point)(void))
{
    task_t *task = task_alloc();
    if (task == NULL)
    {
        return NULL;
    }

    (void)entry_point;
    return task;
}

task_t *task_get_current(void)
{
    return current_task;
}

void task_set_current(task_t *task)
{
    current_task = task;
}
