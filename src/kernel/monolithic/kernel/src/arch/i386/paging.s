.section .text

# load_page_directory - Sets the Page Directory Base Register (CR3)
# stack: [esp + 4] the physical address of the Page Directory
# [esp] the return address
.global load_page_directory
.type load_page_directory, @function
load_page_directory:
    mov 4(%esp), %eax    # Get physical address of PDT from the stack
    mov %eax, %cr3       # Load into CR3 [1]
    ret

# enable_paging - Enables the paging unit by setting the PG bit in CR0
.global enable_paging
.type enable_paging, @function
enable_paging:
    mov %cr0, %eax       # Read current CR0
    or $0x80000000, %eax # Set Bit 31 (PG - Paging Enable bit) [1]
    mov %eax, %cr0       # Update CR0 to enable paging
    ret

# disable_paging - Disables the paging unit by clearing the PG bit in CR0
.global disable_paging
.type disable_paging, @function
disable_paging:
    mov %cr0, %eax
    and $0x7FFFFFFF, %eax # Clear Bit 31
    mov %eax, %cr0
    ret

# invalidate_tlb_entry - Invalidates a single TLB entry for a virtual address
# stack: [esp + 4] the virtual address to invalidate
.global invalidate_tlb_entry
.type invalidate_tlb_entry, @function
invalidate_tlb_entry:
    mov 4(%esp), %eax
    invlpg (%eax)        # Invalidate TLB entry for address in EAX [2]
    ret