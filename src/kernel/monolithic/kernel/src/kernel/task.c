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
#include <kernel/elf_loader.h>
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

void task_enqueue(task_t *task)
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

    if (task->type != TASK_TYPE_KERNEL)
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

static void task_init_process_fields(task_t *task)
{
    task->parent_pid = 0;
    task->parent = NULL;
    task->first_child = NULL;
    task->next_sibling = NULL;
    task->waiting_for_pid = -1;
}

void task_add_child(task_t *parent, task_t *child)
{
    if (parent == NULL || child == NULL)
        return;

    child->parent = parent;
    child->parent_pid = parent->pid;
    child->next_sibling = parent->first_child;
    parent->first_child = child;
}

void task_remove_child(task_t *child)
{
    if (child == NULL || child->parent == NULL)
        return;

    task_t **current = &child->parent->first_child;
    while (*current != NULL)
    {
        if (*current == child)
        {
            *current = child->next_sibling;
            break;
        }
        current = &(*current)->next_sibling;
    }

    child->parent = NULL;
    child->parent_pid = 0;
    child->next_sibling = NULL;
}

task_t *task_find_child(task_t *parent, int pid)
{
    if (parent == NULL)
        return NULL;

    for (task_t *child = parent->first_child; child != NULL; child = child->next_sibling)
    {
        if (child->pid == pid)
            return child;
    }

    return NULL;
}

static int task_setup_user_stack(task_t *task, const char *const argv[], const char *const envp[])
{
    if (task == NULL || task->user_stack_top == NULL || task->user_stack_size == 0)
        return -1;

    uint32_t stack_top = (uint32_t)(uintptr_t)task->user_stack_top;
    uint32_t stack_bottom = stack_top - task->user_stack_size;
    uint32_t stack_page_vaddr = stack_bottom + PAGE_SIZE;
    uint32_t stack_page_paddr = task->user_stack_paddr;
    if (stack_page_paddr == 0)
        return -1;

    vmm_map_kernel_page(VMM_TEMP_PAGE, stack_page_paddr);
    uint8_t *page = (uint8_t *)VMM_TEMP_PAGE;
    memset(page, 0, PAGE_SIZE);

    uint32_t offset = PAGE_SIZE;
    uint32_t arg_ptrs[16];
    uint32_t env_ptrs[16];
    uint32_t argc = 0;
    uint32_t envc = 0;

    if (argv != NULL)
    {
        while (argv[argc] != NULL && argc < 16)
            argc++;
    }

    if (envp != NULL)
    {
        while (envp[envc] != NULL && envc < 16)
            envc++;
    }

    for (int i = envc - 1; i >= 0; --i)
    {
        uint32_t len = (uint32_t)strlen(envp[i]) + 1;
        if (len > offset)
        {
            vmm_unmap_page(VMM_TEMP_PAGE);
            return -1;
        }
        offset -= len;
        for (uint32_t byte = 0; byte < len; ++byte)
        {
            page[offset + byte] = (uint8_t)envp[i][byte];
        }
        env_ptrs[i] = stack_page_vaddr + offset;
    }

    offset &= ~0x3;

    for (int i = argc - 1; i >= 0; --i)
    {
        uint32_t len = (uint32_t)strlen(argv[i]) + 1;
        if (len > offset)
        {
            vmm_unmap_page(VMM_TEMP_PAGE);
            return -1;
        }
        offset -= len;
        for (uint32_t byte = 0; byte < len; ++byte)
        {
            page[offset + byte] = (uint8_t)argv[i][byte];
        }
        arg_ptrs[i] = stack_page_vaddr + offset;
    }

    offset &= ~0x3;

    if ((uint32_t)(envc + 1 + argc + 1) * sizeof(uint32_t) + sizeof(uint32_t) > offset)
    {
        vmm_unmap_page(VMM_TEMP_PAGE);
        return -1;
    }

    /* Push envp pointers */
    offset -= sizeof(uint32_t);
    *(uint32_t *)(page + offset) = 0;
    for (int i = envc - 1; i >= 0; --i)
    {
        offset -= sizeof(uint32_t);
        *(uint32_t *)(page + offset) = env_ptrs[i];
    }

    /* Push argv pointers */
    offset -= sizeof(uint32_t);
    *(uint32_t *)(page + offset) = 0;
    for (int i = argc - 1; i >= 0; --i)
    {
        offset -= sizeof(uint32_t);
        *(uint32_t *)(page + offset) = arg_ptrs[i];
    }

    /* Push argc */
    offset -= sizeof(uint32_t);
    *(uint32_t *)(page + offset) = argc;

    task->context.esp = stack_page_vaddr + offset;
    vmm_unmap_page(VMM_TEMP_PAGE);
    return 0;
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

static int validate_user_pointer(const void *ptr)
{
    uint32_t vaddr = (uint32_t)ptr;
    if (vaddr >= KERNEL_VIRT_BASE)
        return 0;
    return vmm_get_phys_addr(vaddr) != 0;
}

void task_terminate(int status)
{
    if (current_task == NULL)
        return;

    current_task->exit_status = status;
    current_task->state = TASK_ZOMBIE;

    // DO NOT free user resources here - leave for parent to reap via sys_wait

    // Wake parent if waiting
    if (current_task->parent != NULL && current_task->parent->state == TASK_WAITING &&
        (current_task->parent->waiting_for_pid == current_task->pid || current_task->parent->waiting_for_pid == -1))
    {
        uint32_t old_cr3 = read_cr3();
        load_cr3(current_task->parent->page_directory_phys);
        if (current_task->parent->waiting_status != NULL && validate_user_pointer(current_task->parent->waiting_status))
        {
            *current_task->parent->waiting_status = current_task->exit_status;
        }
        load_cr3(old_cr3);
        current_task->parent->context.eax = current_task->pid;
        current_task->parent->waiting_status = NULL;
        current_task->parent->state = TASK_READY;
        current_task->parent->waiting_for_pid = 0;
        task_enqueue(current_task->parent);
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

task_t *task_alloc(void)
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
            task->user_code_frame_count = 0;
            for (uint32_t frame = 0; frame < MAX_USER_CODE_PAGES; ++frame)
            {
                task->user_code_frames[frame] = 0;
            }
            task_init_process_fields(task);
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
        task_table[index].user_code_frame_count = 0;
        for (uint32_t frame = 0; frame < MAX_USER_CODE_PAGES; ++frame)
        {
            task_table[index].user_code_frames[frame] = 0;
        }
        task_init_process_fields(&task_table[index]);
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

task_t *task_create_user_from_elf(const void *elf_data,
                                  size_t elf_size,
                                  uint32_t *user_stack_top,
                                  uint32_t user_stack_size,
                                  const char *const argv[],
                                  const char *const envp[])
{
    if (elf_data == NULL || elf_size == 0 || user_stack_top == NULL || user_stack_size == 0)
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
    vmm_map_page_for_pdt(user_pdt, 0xB8000, 0xB8000, PAGE_PRESENT | PAGE_WRITE | PAGE_USER);

    task->type = TASK_TYPE_USER_PROCESS;
    task->user_stack_top = user_stack_top;
    task->user_stack_size = user_stack_size;
    task->state = TASK_READY;
    task->ticks = 0;
    task->quantum = TASK_QUANTUM_DEFAULT;

    uint32_t user_stack_vaddr = (uint32_t)user_stack_top - user_stack_size;
    printk(LOG_INFO, "task_create_user_from_elf: pid=%u pd=0x%08x stack=0x%08x size=%u", task->pid, user_pdt, user_stack_vaddr, user_stack_size);

    uint32_t user_stack_paddr = (uint32_t)pmm_alloc_frame();
    if (user_stack_paddr == 0)
    {
        task->state = TASK_UNUSED;
        return NULL;
    }

    task->user_stack_paddr = user_stack_paddr;
    vmm_map_page_for_pdt(user_pdt, user_stack_vaddr + PAGE_SIZE, user_stack_paddr, PAGE_PRESENT | PAGE_WRITE | PAGE_USER);

    uint32_t page_frames[MAX_USER_CODE_PAGES];
    uint32_t page_count = 0;
    uint32_t entry_point = 0;

    if (load_elf(elf_data, elf_size, user_pdt, &entry_point, page_frames, &page_count) != 0)
    {
        pmm_free_frame((void *)task->user_stack_paddr);
        task->user_stack_paddr = 0;
        task->state = TASK_UNUSED;
        return NULL;
    }

    task->user_code_frame_count = page_count;
    task->user_code_paddr = page_count > 0 ? page_frames[0] : 0;
    for (uint32_t i = 0; i < page_count; ++i)
    {
        task->user_code_frames[i] = page_frames[i];
    }

    if (task_setup_user_stack(task, argv, envp) != 0)
    {
        if (task->user_stack_paddr != 0)
        {
            pmm_free_frame((void *)task->user_stack_paddr);
            task->user_stack_paddr = 0;
        }
        for (uint32_t i = 0; i < task->user_code_frame_count; ++i)
        {
            if (task->user_code_frames[i] != 0)
                pmm_free_frame((void *)(uintptr_t)task->user_code_frames[i]);
            task->user_code_frames[i] = 0;
        }
        task->user_code_frame_count = 0;
        task->state = TASK_UNUSED;
        return NULL;
    }

    task_create_context(task, (void (*)(void))(uintptr_t)entry_point);
    task->context.eip = entry_point;

    task_enqueue(task);
    return task;
}

int task_replace_process_image(task_t *task,
                               const void *elf_data,
                               size_t elf_size,
                               const char *const argv[],
                               const char *const envp[])
{
    if (task == NULL || task->type != TASK_TYPE_USER_PROCESS)
        return -1;

    for (uint32_t i = 0; i < task->user_code_frame_count; ++i)
    {
        if (task->user_code_frames[i] != 0)
        {
            pmm_free_frame((void *)(uintptr_t)task->user_code_frames[i]);
            task->user_code_frames[i] = 0;
        }
    }
    task->user_code_frame_count = 0;
    task->user_code_paddr = 0;

    if (task->user_stack_paddr != 0)
    {
        pmm_free_frame((void *)task->user_stack_paddr);
        task->user_stack_paddr = 0;
    }

    uint32_t user_stack_vaddr = (uint32_t)task->user_stack_top - task->user_stack_size;
    uint32_t user_stack_paddr = (uint32_t)pmm_alloc_frame();
    if (user_stack_paddr == 0)
        return -1;

    task->user_stack_paddr = user_stack_paddr;
    vmm_map_page_for_pdt(task->page_directory_phys, user_stack_vaddr + PAGE_SIZE, user_stack_paddr, PAGE_PRESENT | PAGE_WRITE | PAGE_USER);

    uint32_t page_frames[MAX_USER_CODE_PAGES];
    uint32_t page_count = 0;
    uint32_t entry_point = 0;

    if (load_elf(elf_data, elf_size, task->page_directory_phys, &entry_point, page_frames, &page_count) != 0)
    {
        pmm_free_frame((void *)task->user_stack_paddr);
        task->user_stack_paddr = 0;
        return -1;
    }

    task->user_code_frame_count = page_count;
    task->user_code_paddr = page_count > 0 ? page_frames[0] : 0;
    for (uint32_t i = 0; i < page_count; ++i)
    {
        task->user_code_frames[i] = page_frames[i];
    }

    if (task_setup_user_stack(task, argv, envp) != 0)
    {
        if (task->user_stack_paddr != 0)
        {
            pmm_free_frame((void *)task->user_stack_paddr);
            task->user_stack_paddr = 0;
        }
        for (uint32_t i = 0; i < task->user_code_frame_count; ++i)
        {
            if (task->user_code_frames[i] != 0)
                pmm_free_frame((void *)(uintptr_t)task->user_code_frames[i]);
            task->user_code_frames[i] = 0;
        }
        task->user_code_frame_count = 0;
        return -1;
    }

    task_create_context(task, (void (*)(void))(uintptr_t)entry_point);
    task->context.eip = entry_point;
    return 0;
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
