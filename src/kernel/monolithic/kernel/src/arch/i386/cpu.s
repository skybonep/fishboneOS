.section .text

.global read_cr0
.type read_cr0, @function
read_cr0:
    mov %cr0, %eax    # Move CR0 to EAX (return register)
    ret

.global read_cr2
.type read_cr2, @function
read_cr2:
    mov %cr2, %eax    # Move CR2 to EAX (return register)
    ret

.global read_cr3
.type read_cr3, @function
read_cr3:
    mov %cr3, %eax    # Move CR3 to EAX
    ret

.global read_cr4
.type read_cr4, @function
read_cr4:
    mov %cr4, %eax    # Move CR4 to EAX
    ret

.global read_ebx
.type read_ebx, @function
read_ebx:
    mov %ebx, %eax    # EBX contains Multiboot pointer
    ret

.global read_esp
.type read_esp, @function
read_esp:
    mov %esp, %eax    # Current Stack Pointer
    ret

.global read_ebp
.type read_ebp, @function
read_ebp:
    mov %ebp, %eax    # Current Base Pointer
    ret

.global load_gdt
.type load_gdt, @function
load_gdt:
    mov 4(%esp), %eax    # Get the pointer to gdt_ptr from the stack
    lgdt (%eax)          # Load the GDT

    # Reload data segment registers with Kernel Data Segment (0x10) (2 * 8 bytes)
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss

    # Perform a far jump to reload CS with Kernel Code Segment (0x08)
    ljmp $0x08, $.flush
.flush:
    ret

.global load_tss
.type load_tss, @function
load_tss:
    mov 4(%esp), %ax     # Get the TSS selector from the stack
    ltr %ax              # Load the Task Register
    ret

.global load_idt
.type load_idt, @function
load_idt:
    # Get the pointer to idt_ptr from the stack.
    # [esp] is the return address, [esp + 4] is the first argument.
    mov 4(%esp), %eax

    # Load the IDT using the 'lidt' instruction.
    # The CPU expects the address of the 6-byte idt_ptr structure.
    lidt (%eax)

    # Return to the calling C function (idt_init)
    ret

.macro no_error_code_interrupt_handler num
.global interrupt_handler_\num
interrupt_handler_\num:
    push $0                     # Push dummy error code
    push \num                  # Push the interrupt number
    jmp common_interrupt_handler # Jump to the shared logic
.endm

# Define the specific handler for the timer (32)
no_error_code_interrupt_handler 32

# Define the specific handler for the keyboard (33)
no_error_code_interrupt_handler 33

# Define the syscall handler for INT 0x80 (128)
no_error_code_interrupt_handler 128

# The common handler saves the state and calls C
common_interrupt_handler:
    pusha                       # Save eax, ebx, ecx, edx, esp, ebp, esi, edi

    mov %esp, %ecx              # Capture pointer to saved EDI..EAX register frame
    push %ecx                   # Pass pointer to saved cpu state for task preemption
    call task_save_current_context
    add $4, %esp

    mov %esp, %ecx              # Recompute pointer after the call returns
    push %ecx                   # Pass raw saved cpu state pointer to interrupt_handler
    call interrupt_handler
    add $4, %esp

    test %eax, %eax             # eax == 0 means no task switch
    jz .restore_current_task

    jmp task_resume             # Resume the next task using its saved context

.restore_current_task:
    popa                        # Restore current task registers
    add $8, %esp                # Clean up the interrupt number and dummy error code
    iret                        # Return to the interrupted code

.global task_resume
.type task_resume, @function
task_resume:
    mov %eax, %edi              # EDI = pointer to task_context_t
    mov 36(%edi), %ecx          # Target CS from task context
    cmp $0x1B, %ecx             # USER_CODE_SEG
    jne .resume_kernel

    # User-mode resume: build a full ring transition iret frame.
    mov 12(%edi), %edx          # Target user ESP
    sub $20, %edx
    mov %edx, %esp

    mov 32(%edi), %eax          # Saved EIP
    mov %eax, 0(%esp)
    mov 36(%edi), %eax          # Saved CS
    mov %eax, 4(%esp)
    mov 44(%edi), %eax          # Saved EFLAGS
    mov %eax, 8(%esp)
    mov 12(%edi), %eax          # User ESP
    mov %eax, 12(%esp)
    mov 40(%edi), %eax          # Saved SS
    mov %eax, 16(%esp)

    mov 0(%edi), %edi
    mov 4(%edi), %esi
    mov 8(%edi), %ebp
    mov 16(%edi), %ebx
    mov 20(%edi), %edx
    mov 24(%edi), %ecx
    mov 28(%edi), %eax
    iret

.resume_kernel:
    mov 12(%edi), %esp

    mov 32(%edi), %eax          # Saved EIP
    mov %eax, 0(%esp)
    mov 36(%edi), %eax          # Saved CS
    mov %eax, 4(%esp)
    mov 44(%edi), %eax          # Saved EFLAGS
    mov %eax, 8(%esp)

    mov 0(%edi), %edi
    mov 4(%edi), %esi
    mov 8(%edi), %ebp
    mov 16(%edi), %ebx
    mov 20(%edi), %edx
    mov 24(%edi), %ecx
    mov 28(%edi), %eax
    iret
