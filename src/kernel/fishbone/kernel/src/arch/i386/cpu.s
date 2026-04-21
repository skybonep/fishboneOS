.section .text

.global load_gdt
.type load_gdt, @function
load_gdt:
    mov 4(%esp), %eax    # Get the address of the GDT pointer
    lgdt (%eax)          # Load the GDT
    mov $0x10, %ax       # 0x10 is the offset in the GDT to our data segment
    mov %ax, %ds         # Load all data segment selectors
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss
    jmp $0x08, $flush    # 0x08 is the offset to our code segment: Far jump!
flush:
    ret

.global load_tss
.type load_tss, @function
load_tss:
    mov $0x2B, %ax       # Load the index of our TSS structure - The index is
                         # 0x28, as it is the 5th selector and each is 8 bytes
                         # long, but we set the bottom two bits (making 0x2B)
                         # so that it has an RPL of 3, not zero.
    ltr %ax              # Load task register
    ret

.global load_idt
.type load_idt, @function
load_idt:
    mov 4(%esp), %eax    # Get the address of the IDT pointer
    lidt (%eax)          # Load the IDT
    ret

# Interrupt handler for divide by zero (interrupt 0)
.global interrupt_handler_0
.type interrupt_handler_0, @function
interrupt_handler_0:
    pushal
    call divide_by_zero_handler
    popal
    # Skip the faulting instruction (div %eax is 2 bytes)
    addl $2, (%esp)
    iret
