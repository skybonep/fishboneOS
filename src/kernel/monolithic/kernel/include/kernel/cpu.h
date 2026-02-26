#ifndef KERNEL_CPU_H
#define KERNEL_CPU_H

/* Prototypes for assembly functions in info.s */
unsigned int read_cr0(void);
unsigned int read_cr2(void);
unsigned int read_cr3(void);
unsigned int read_cr4(void);
unsigned int read_ebx(void);
unsigned int read_esp(void);
unsigned int read_ebp(void);

/* CR0 Bit Definitions [4] */
#define CR0_PE 0x00000001 // Protection Enable (Bit 0)
#define CR0_PG 0x80000000 // Paging Enable (Bit 31)

#endif