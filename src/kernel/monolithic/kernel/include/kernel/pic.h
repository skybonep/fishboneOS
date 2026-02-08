#ifndef PIC_H
#define PIC_H

void pic_remap(void);
void pic_disable_all_irq(void);
void pic_enable_irq(unsigned char irq);
void pic_sendEOI(unsigned int interrupt);

#endif