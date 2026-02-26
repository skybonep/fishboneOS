#ifndef KERNEL_INFO_H
#define KERNEL_INFO_H

void log_system_info(void);

void multiboot_info(unsigned int multiboot_magic, multiboot_info_t *mbinfo);

#endif