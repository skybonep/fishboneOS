#include <kernel/cpu.h>
#include <kernel/log.h>

/* Minimal helper to send strings to the log */
void log_string(char *str) {
    kprint(LOG_INFO, str);
}

void log_system_info(void) {
    unsigned int cr0 = read_cr0();
    unsigned int ebx = read_ebx();

    log_string("--- fishboneOS System Info ---");

    /* Check Bit 0 of CR0 for Protected Mode */
    if (cr0 & CR0_PE) {
        log_string("Processor Mode: 32-bit Protected Mode");
    } else {
        log_string("Processor Mode: 16-bit Real Mode");
    }

    /* Check Bit 31 of CR0 for Paging status */
    if (cr0 & CR0_PG) {
        log_string("Paging: ENABLED");
    } else {
        log_string("Paging: DISABLED");
    }

    /* Multiboot Check: If EBX is null, we likely aren't using GRUB */
    if (ebx != 0) {
        log_string("Bootloader: Multiboot compatible (GRUB detected)");
    } else {
        log_string("Bootloader: Unknown (Manual Boot)");
    }

    log_string("------------------------------");
}