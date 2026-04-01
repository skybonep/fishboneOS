#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <kernel/memory_map.h>
#include <kernel/task.h>
#include <kernel/log.h>

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
    task_context_t *context = &task->context;

    context->edi = 0;
    context->esi = 0;
    context->ebp = 0;
    context->esp = (uint32_t)(uintptr_t)task->stack_top - 32;
    context->ebx = 0;
    context->edx = 0;
    context->ecx = 0;
    context->eax = 0;

    context->eip = (uint32_t)(uintptr_t)entry_point;
    context->cs = 0x08;
    context->eflags = 0x202;

    printk(LOG_DEBUG, "SCHED DEBUG: create pid=%u stack_base=0x%08x stack_top=0x%08x context=%p esp=0x%08x",
           task->pid, (uint32_t)task->stack_base, (uint32_t)task->stack_top,
           (void *)context, context->esp);
    return context;
}

void task_save_current_context(void *cpu_state_ptr)
{
    if (current_task == NULL || current_task->state != TASK_RUNNING || cpu_state_ptr == NULL)
    {
        return;
    }

    task_context_t *context = &current_task->context;
    uint32_t *saved_regs = (uint32_t *)cpu_state_ptr;
    uint32_t saved_esp = saved_regs[3];
    uint32_t saved_intno = saved_regs[8];
    uint32_t saved_err = saved_regs[9];
    uint32_t saved_eip = saved_regs[10];
    uint32_t saved_cs = saved_regs[11];
    uint32_t saved_eflags = saved_regs[12];

    printk(LOG_DEBUG, "SCHED DEBUG: pid=%u context=%p stack_base=0x%08x stack_top=0x%08x saved_regs[0]=0x%08x saved_regs[1]=0x%08x saved_regs[2]=0x%08x saved_regs[3]=0x%08x saved_regs[4]=0x%08x saved_regs[5]=0x%08x saved_regs[6]=0x%08x saved_regs[7]=0x%08x saved_regs[8]=0x%08x saved_regs[9]=0x%08x saved_regs[10]=0x%08x saved_regs[11]=0x%08x saved_regs[12]=0x%08x int=%u err=%u raw_eip=0x%08x raw_cs=0x%08x raw_eflags=0x%08x",
           current_task->pid, (void *)context,
           (uint32_t)current_task->stack_base, (uint32_t)current_task->stack_top,
           saved_regs[0], saved_regs[1], saved_regs[2], saved_regs[3], saved_regs[4], saved_regs[5], saved_regs[6], saved_regs[7],
           saved_regs[8], saved_regs[9], saved_regs[10], saved_regs[11], saved_regs[12],
           saved_intno, saved_err, saved_eip, saved_cs, saved_eflags);

    context->edi = saved_regs[0];
    context->esi = saved_regs[1];
    context->ebp = saved_regs[2];
    context->esp = saved_esp + 20; /* resume at the original task stack pointer */
    context->ebx = saved_regs[4];
    context->edx = saved_regs[5];
    context->ecx = saved_regs[6];
    context->eax = saved_regs[7];

    uint32_t *return_frame = saved_regs + 10;
    context->eip = return_frame[0];
    context->cs = return_frame[1];
    context->eflags = return_frame[2];

    printk(LOG_DEBUG, "SCHED DEBUG POST: pid=%u ctx=%p edi=0x%08x esi=0x%08x ebp=0x%08x esp=0x%08x ebx=0x%08x edx=0x%08x ecx=0x%08x eax=0x%08x eip=0x%08x cs=0x%08x eflags=0x%08x",
           current_task->pid, (void *)context,
           context->edi, context->esi, context->ebp,
           context->esp, context->ebx, context->edx,
           context->ecx, context->eax, context->eip,
           context->cs, context->eflags);
    printk(LOG_DEBUG, "SCHED: saved context for pid=%u eip=0x%08x esp=0x%08x", current_task->pid, context->eip, context->esp);
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
            memset(&task->context, 0, sizeof(task_context_t));
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
        memset(&task_table[index].context, 0, sizeof(task_context_t));
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

    return &next->context;
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
        task_t *old_task = current_task;
        current_task->state = TASK_READY;
        task_enqueue(current_task);
        task_t *next = task_select_next();
        if (next == NULL)
        {
            return NULL;
        }

        printk(LOG_INFO, "SCHED: switch pid=%u -> pid=%u", old_task->pid, next->pid);
        return &next->context;
    }

    return NULL;
}

task_t *task_get_current(void)
{
    return current_task;
}

void task_set_current(task_t *task)
{
    if (task == current_task)
    {
        return;
    }

    if (task != NULL && task->state == TASK_READY)
    {
        task_remove(task);
    }

    current_task = task;
    if (current_task != NULL)
    {
        current_task->state = TASK_RUNNING;
        current_task->ticks = 0;
    }
}
