.section .text

.global read_cr0
.type read_cr0, @function
read_cr0:
    mov %cr0, %eax    # Move CR0 to EAX (return register)
    ret

.global read_cr3
.type read_cr3, @function
read_cr3:
    mov %cr3, %eax    # Move CR3 to EAX
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