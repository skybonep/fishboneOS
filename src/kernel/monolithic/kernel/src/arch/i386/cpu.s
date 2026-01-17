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
    mov %ebx, %eax    # EBX contains Multiboot pointer [4]
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