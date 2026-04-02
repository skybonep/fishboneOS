#include <stddef.h>
#include <stddef.h>
#include <kernel/pic.h>
#include <kernel/task.h>
#include <drivers/timer.h>
#include <drivers/keyboard.h>
#include <kernel/log.h>

#define KBD_DATA_PORT 0x60

struct cpu_state
{
    unsigned int eax, ecx, edx, ebx, esp, ebp, esi, edi;
} __attribute__((packed));

struct stack_state
{
    unsigned int error_code;
    unsigned int eip;
    unsigned int cs;
    unsigned int eflags;
} __attribute__((packed));

/**
 * dump_registers:
 * A private "Core Dump" utility that captures the absolute CPU state.
 * Consolidates general registers and a full EFLAGS bit-breakdown
 * into a single serial log entry.
 */
static void dump_registers(struct cpu_state cpu, struct stack_state stack, unsigned int interrupt)
{
    printk(LOG_FATAL,
           "\n--- KERNEL PANIC: Exception %d ---\n"
           "EAX: %08x EBX: %08x ECX: %08x EDX: %08x\n"
           "ESI: %08x EDI: %08x EBP: %08x ESP: %08x\n"
           "EIP: %08x  CS: %08x EFLAGS: %08x\n"
           "FLAGS: [ CF:%d PF:%d AF:%d ZF:%d SF:%d TF:%d IF:%d DF:%d OF:%d ]\n"
           "SYSTEM: [ IOPL:%d NT:%d RF:%d VM:%d AC:%d VIF:%d VIP:%d ID:%d ]\n"
           "ERROR CODE: %d\n"
           "--- SYSTEM HALT ---",
           // Basic Info and Registers
           interrupt, cpu.eax, cpu.ebx, cpu.ecx, cpu.edx,
           cpu.esi, cpu.edi, cpu.ebp, cpu.esp,
           stack.eip, stack.cs, stack.eflags,
           // EFLAGS Status/Control Bits
           (stack.eflags >> 0) & 1,  // Carry
           (stack.eflags >> 2) & 1,  // Parity
           (stack.eflags >> 4) & 1,  // Auxiliary Carry
           (stack.eflags >> 6) & 1,  // Zero
           (stack.eflags >> 7) & 1,  // Sign
           (stack.eflags >> 8) & 1,  // Trap
           (stack.eflags >> 9) & 1,  // Interrupt
           (stack.eflags >> 10) & 1, // Direction
           (stack.eflags >> 11) & 1, // Overflow
           // EFLAGS System/Advanced Bits
           (stack.eflags >> 12) & 3, // IOPL (2-bits)
           (stack.eflags >> 14) & 1, // Nested Task
           (stack.eflags >> 16) & 1, // Resume
           (stack.eflags >> 17) & 1, // Virtual-8086
           (stack.eflags >> 18) & 1, // Alignment Check
           (stack.eflags >> 19) & 1, // Virtual Interrupt
           (stack.eflags >> 20) & 1, // Virtual Interrupt Pending
           (stack.eflags >> 21) & 1, // ID (CPUID Support)
           // Final State
           stack.error_code);

    // Enter the infinite loop to halt the system [5].
    while (1)
        ;
}

void *interrupt_handler(void *cpu_state_ptr)
{
    if (cpu_state_ptr == NULL)
    {
        return NULL;
    }

    uint32_t *saved_regs = (uint32_t *)cpu_state_ptr;
    struct cpu_state cpu;
    cpu.eax = saved_regs[7];
    cpu.ecx = saved_regs[6];
    cpu.edx = saved_regs[5];
    cpu.ebx = saved_regs[4];
    cpu.esp = saved_regs[3];
    cpu.ebp = saved_regs[2];
    cpu.esi = saved_regs[1];
    cpu.edi = saved_regs[0];

    unsigned int interrupt = saved_regs[8];
    struct stack_state stack;
    stack.error_code = saved_regs[9];
    stack.eip = saved_regs[10];
    stack.cs = saved_regs[11];
    stack.eflags = saved_regs[12];

    // 1. Handle CPU Exceptions (0-31)
    if (interrupt < 32)
    {
        // This is a fault/exception. Dump registers and halt.
        dump_registers(cpu, stack, interrupt);
        return NULL;
    }

    // 2. Handle Hardware Interrupts (32+)
    void *next_context = NULL;

    if (interrupt == 32)
    {
        timer_handle_interrupt();
        next_context = task_tick();
    }
    else if (interrupt == 33)
    {
        keyboard_handle_interrupt();
    }
    else
    {
        // Unhandled hardware IRQ: still acknowledge it to prevent locking the PIC.
    }

    // Always acknowledge hardware interrupts to the PIC [10, 11]
    pic_sendEOI(interrupt);
    return next_context;
}
