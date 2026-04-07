#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <kernel/memory_map.h>
#include <kernel/gdt.h>
#include <kernel/cpu.h>
#include <kernel/vmm.h>
#include <kernel/pmm.h>
#include <kernel/paging.h>
#include <drivers/timer.h>
#include <kernel/task.h>
#include <kernel/log.h>

extern struct tss_entry tss;

static task_t task_table[TASK_MAX];
static task_t *current_task = NULL;
static task_t *ready_list_head = NULL;
static task_t *ready_list_tail = NULL;
static uint32_t next_pid = 1;
static bool task_stack_slots[TASK_MAX];

static void task_update_tss(void)
{
    if (current_task != NULL)
    {
        tss.esp0 = (uint32_t)current_task->stack_top;
        tss.ss0 = KERNEL_DATA_SEG;
    }
}

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
        printk(LOG_INFO, "task_select_next: no ready task");
        return NULL;
    }

    task_remove(next);
    next->state = TASK_RUNNING;
    next->ticks = 0;
    current_task = next;

    // printk(LOG_INFO, "task_select_next: switch to pid=%u type=%u pd=0x%08x", next->pid, next->type, next->page_directory_phys);

    task_update_tss();

    if (current_task != NULL && current_task->page_directory_phys != 0)
    {
        load_cr3(current_task->page_directory_phys);
    }
    else
    {
        load_cr3(vmm_get_kernel_pdt_phys());
    }

    return next;
}

static task_context_t *task_create_context(task_t *task, void (*entry_point)(void))
{
    task_context_t *context = &task->context;

    context->edi = 0;
    context->esi = 0;
    context->ebp = 0;
    context->ebx = 0;
    context->edx = 0;
    context->ecx = 0;
    context->eax = 0;
    context->eip = (uint32_t)(uintptr_t)entry_point;
    context->eflags = 0x202;

    if (task->type == TASK_TYPE_USER)
    {
        context->esp = (uint32_t)(uintptr_t)task->user_stack_top - 32;
        context->cs = USER_CODE_SEG;
        context->ss = USER_DATA_SEG;
    }
    else
    {
        context->esp = (uint32_t)(uintptr_t)task->stack_top - 32;
        context->cs = KERNEL_CODE_SEG;
        context->ss = KERNEL_DATA_SEG;
    }

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

    context->edi = saved_regs[0];
    context->esi = saved_regs[1];
    context->ebp = saved_regs[2];
    context->ebx = saved_regs[4];
    context->edx = saved_regs[5];
    context->ecx = saved_regs[6];
    context->eax = saved_regs[7];

    /* The return frame starts after pusha, the interrupt number, and the dummy error code. */
    uint32_t *return_frame = saved_regs + 10;
    if (current_task->type == TASK_TYPE_USER)
    {
        /* For ring3 interrupt entry, the stack frame is: SS, ESP, EFLAGS, CS, EIP. */
        context->ss = return_frame[0];
        context->esp = return_frame[1];
        context->eflags = return_frame[2];
        context->cs = return_frame[3];
        context->eip = return_frame[4];
    }
    else
    {
        context->eip = return_frame[0];
        context->cs = return_frame[1];
        context->eflags = return_frame[2];

        context->esp = saved_esp; /* resume at the original task stack pointer */
        context->ss = KERNEL_DATA_SEG;
    }
}

static uint32_t task_stack_slot_count(void)
{
    uint32_t region_size = KERNEL_STACK_REGION_END - KERNEL_STACK_REGION_START + 1;
    uint32_t slot_count = region_size / KERNEL_STACK_SIZE;
    return slot_count > TASK_MAX ? TASK_MAX : slot_count;
}

static void task_wake_waiting_tasks(void)
{
    uint32_t now = timer_get_ticks();

    for (uint32_t index = 0; index < TASK_MAX; ++index)
    {
        task_t *task = &task_table[index];
        if (task->state == TASK_WAITING && task->wake_tick != 0 && task->wake_tick <= now)
        {
            task->state = TASK_READY;
            task->wake_tick = 0;
            task_enqueue(task);
        }
    }
}

void task_terminate(int status)
{
    if (current_task == NULL)
        return;

    current_task->exit_status = status;
    current_task->state = TASK_ZOMBIE;

    // Free user resources if user task
    if (current_task->type == TASK_TYPE_USER)
    {
        if (current_task->user_stack_paddr != 0)
        {
            pmm_free_frame((void *)current_task->user_stack_paddr);
            current_task->user_stack_paddr = 0;
        }
        if (current_task->user_code_paddr != 0)
        {
            pmm_free_frame((void *)current_task->user_code_paddr);
            current_task->user_code_paddr = 0;
        }
    }
}

void task_exit(int status)
{
    task_terminate(status);
    task_schedule();
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
            task->type = TASK_TYPE_KERNEL;
            task->quantum = TASK_QUANTUM_DEFAULT;
            task->ticks = 0;
            task->stack_base = NULL;
            task->stack_top = NULL;
            task->stack_size = 0;
            task->user_stack_top = NULL;
            task->user_stack_size = 0;
            task->user_stack_paddr = 0;
            task->user_code_paddr = 0;
            task->wake_tick = 0;
            task->exit_status = 0;
            task->page_directory_phys = vmm_get_kernel_pdt_phys(); /* default to kernel PD */
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
        task_table[index].type = TASK_TYPE_KERNEL;
        task_table[index].quantum = 0;
        task_table[index].ticks = 0;
        task_table[index].stack_base = NULL;
        task_table[index].stack_top = NULL;
        task_table[index].stack_size = 0;
        task_table[index].user_stack_top = NULL;
        task_table[index].user_stack_size = 0;
        task_table[index].user_stack_paddr = 0;
        task_table[index].user_code_paddr = 0;
        task_table[index].wake_tick = 0;
        task_table[index].exit_status = 0;
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

    task->type = TASK_TYPE_KERNEL;
    task->state = TASK_READY;
    task->ticks = 0;
    task->quantum = TASK_QUANTUM_DEFAULT;
    task_create_context(task, entry_point);
    task_enqueue(task);
    return task;
}

task_t *task_create_user(void (*entry_point)(void), uint32_t *user_stack_top, uint32_t user_stack_size)
{
    if (user_stack_top == NULL || user_stack_size == 0)
    {
        return NULL;
    }

    task_t *task = task_alloc();
    if (task == NULL)
    {
        return NULL;
    }

    uint32_t user_pdt = vmm_clone_kernel_mappings();
    if (user_pdt == 0)
    {
        task->state = TASK_UNUSED;
        return NULL;
    }

    task->page_directory_phys = user_pdt;

    // Map VGA memory for user access
    vmm_map_page_for_pdt(user_pdt, 0xB8000, 0xB8000, PAGE_PRESENT | PAGE_WRITE | PAGE_USER);

    task->type = TASK_TYPE_USER;
    task->user_stack_top = user_stack_top;
    task->user_stack_size = user_stack_size;
    task->state = TASK_READY;
    task->ticks = 0;
    task->quantum = TASK_QUANTUM_DEFAULT;

    uint32_t user_stack_vaddr = (uint32_t)user_stack_top - user_stack_size;

    printk(LOG_INFO, "task_create_user: pid=%u pd=0x%08x stack=0x%08x size=%u", task->pid, user_pdt, user_stack_vaddr, user_stack_size);

    uint32_t user_stack_paddr = (uint32_t)pmm_alloc_frame();
    if (user_stack_paddr == 0)
    {
        task->state = TASK_UNUSED;
        return NULL;
    }

    task->user_stack_paddr = user_stack_paddr;

    /* allocate a page in user address space for user code */
    const uint32_t user_code_vaddr = 0x00400000; /* user-space entry address */
    uint32_t user_code_paddr = (uint32_t)pmm_alloc_frame();
    if (user_code_paddr == 0)
    {
        task->state = TASK_UNUSED;
        return NULL;
    }

    task->user_code_paddr = user_code_paddr;

    /* Copy user code into a kernel-temporary mapping first, then map frame in user PD. */
    const uint32_t kernel_temp_vaddr = 0xC0800000;
    vmm_map_kernel_page(kernel_temp_vaddr, user_code_paddr);

    {
        uint8_t *dest = (uint8_t *)kernel_temp_vaddr;
        uint8_t *src = (uint8_t *)entry_point;
        for (uint32_t i = 0; i < PAGE_SIZE; ++i)
        {
            dest[i] = src[i];
        }
    }

    vmm_unmap_page(kernel_temp_vaddr);

    /* map user stack and user code into this task's PD */
    vmm_map_page_for_pdt(user_pdt, user_stack_vaddr + PAGE_SIZE, user_stack_paddr, PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
    vmm_map_page_for_pdt(user_pdt, user_code_vaddr, user_code_paddr, PAGE_PRESENT | PAGE_USER);

    task_create_context(task, entry_point);
    task->context.eip = user_code_vaddr;

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

task_context_t *task_yield(void)
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
    task_wake_waiting_tasks();

    if (current_task == NULL || current_task->state != TASK_RUNNING)
    {
        return NULL;
    }

    current_task->ticks++;
    if (current_task->ticks >= current_task->quantum)
    {
#ifdef DEBUG
        task_t *old_task = current_task;
#endif
        current_task->state = TASK_READY;
        task_enqueue(current_task);
        task_t *next = task_select_next();
        if (next == NULL)
        {
            return NULL;
        }

#ifdef DEBUG
        printk(LOG_DEBUG, "SCHED: switch pid=%u -> pid=%u", old_task->pid, next->pid);
#endif
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

    task_update_tss();

    if (current_task != NULL && current_task->page_directory_phys != 0)
    {
        load_cr3(current_task->page_directory_phys);
    }
    else
    {
        load_cr3(vmm_get_kernel_pdt_phys());
    }
}

task_t *task_get_at_index(uint32_t index)
{
    if (index >= TASK_MAX)
        return NULL;
    return &task_table[index];
}
