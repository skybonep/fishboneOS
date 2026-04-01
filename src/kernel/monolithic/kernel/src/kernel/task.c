#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <kernel/memory_map.h>
#include <kernel/task.h>

static task_t task_table[TASK_MAX];
static task_t *current_task = NULL;
static uint32_t next_pid = 1;
static bool task_stack_slots[TASK_MAX];

static uint32_t task_stack_slot_count(void)
{
    uint32_t region_size = KERNEL_STACK_REGION_END - KERNEL_STACK_REGION_START + 1;
    uint32_t slot_count = region_size / KERNEL_STACK_SIZE;
    return slot_count > TASK_MAX ? TASK_MAX : slot_count;
}

static bool task_allocate_stack(task_t *task)
{
    uint32_t slot_count = task_stack_slot_count();

    for (uint32_t slot = 0; slot < slot_count; ++slot)
    {
        if (!task_stack_slots[slot])
        {
            task_stack_slots[slot] = true;
            uintptr_t stack_base = KERNEL_STACK_REGION_START + (slot * KERNEL_STACK_SIZE);
            task->stack_base = (uint32_t *)stack_base;
            task->stack_top = (uint32_t *)(stack_base + KERNEL_STACK_SIZE);
            task->stack_size = KERNEL_STACK_SIZE;
            return true;
        }
    }

    return false;
}

static void task_free_stack(task_t *task)
{
    if (task->stack_base == NULL)
    {
        return;
    }

    uintptr_t stack_base = (uintptr_t)task->stack_base;
    uint32_t slot = (stack_base - KERNEL_STACK_REGION_START) / KERNEL_STACK_SIZE;

    if (slot < task_stack_slot_count())
    {
        task_stack_slots[slot] = false;
    }

    task->stack_base = NULL;
    task->stack_top = NULL;
    task->stack_size = 0;
}

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

            if (!task_allocate_stack(task))
            {
                task->state = TASK_UNUSED;
                return NULL;
            }

            return task;
        }
    }

    return NULL;
}

void task_init(void)
{
    for (uint32_t index = 0; index < TASK_MAX; ++index)
    {
        task_free_stack(&task_table[index]);
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
        task_stack_slots[index] = false;
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
