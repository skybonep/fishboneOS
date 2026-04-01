#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <kernel/memory_map.h>
#include <kernel/task.h>

static task_t task_table[TASK_MAX];
static task_t *current_task = NULL;
static task_t *ready_list_head = NULL;
static task_t *ready_list_tail = NULL;
static uint32_t next_pid = 1;
static bool task_stack_slots[TASK_MAX];

static void task_enqueue(task_t *task)
{
    task->next = NULL;
    task->prev = ready_list_tail;

    if (ready_list_tail != NULL)
    {
        ready_list_tail->next = task;
    }
    else
    {
        ready_list_head = task;
    }

    ready_list_tail = task;
    task->state = TASK_READY;
}

static void task_remove(task_t *task)
{
    if (task->prev != NULL)
    {
        task->prev->next = task->next;
    }
    else
    {
        ready_list_head = task->next;
    }

    if (task->next != NULL)
    {
        task->next->prev = task->prev;
    }
    else
    {
        ready_list_tail = task->prev;
    }

    task->next = NULL;
    task->prev = NULL;
}

static task_t *task_select_next(void)
{
    task_t *next = ready_list_head;
    if (next == NULL)
    {
        return NULL;
    }

    task_remove(next);
    next->state = TASK_RUNNING;
    next->ticks = 0;
    current_task = next;
    return next;
}

static task_context_t *task_create_context(task_t *task, void (*entry_point)(void))
{
    uintptr_t context_top = (uintptr_t)task->stack_top;
    uintptr_t context_base = context_top - sizeof(task_context_t);
    task_context_t *context = (task_context_t *)context_base;

    context->edi = 0;
    context->esi = 0;
    context->ebp = 0;
    context->esp = (uint32_t)(uintptr_t)&context->eip;
    context->ebx = 0;
    context->edx = 0;
    context->ecx = 0;
    context->eax = 0;

    context->eip = (uint32_t)(uintptr_t)entry_point;
    context->cs = 0x08;
    context->eflags = 0x202;

    task->context = context;
    return context;
}

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

    ready_list_head = NULL;
    ready_list_tail = NULL;
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

    task->state = TASK_READY;
    task->ticks = 0;
    task->quantum = TASK_QUANTUM_DEFAULT;
    task_create_context(task, entry_point);
    task_enqueue(task);
    return task;
}

task_context_t *task_schedule(void)
{
    if (current_task != NULL && current_task->state == TASK_RUNNING)
    {
        current_task->state = TASK_READY;
        task_enqueue(current_task);
    }

    task_t *next = task_select_next();
    if (next == NULL)
    {
        return NULL;
    }

    return next->context;
}

task_context_t *task_tick(void)
{
    if (current_task == NULL || current_task->state != TASK_RUNNING)
    {
        return NULL;
    }

    current_task->ticks++;
    if (current_task->ticks >= current_task->quantum)
    {
        current_task->state = TASK_READY;
        task_enqueue(current_task);
        task_t *next = task_select_next();
        if (next == NULL)
        {
            return NULL;
        }

        return next->context;
    }

    return NULL;
}

task_t *task_get_current(void)
{
    return current_task;
}

void task_set_current(task_t *task)
{
    current_task = task;
}
