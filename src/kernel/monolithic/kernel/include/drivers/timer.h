#ifndef DRIVERS_TIMER_H
#define DRIVERS_TIMER_H

#include <stdint.h>

void timer_init(uint32_t frequency);
void timer_handle_interrupt(void);
uint32_t timer_get_ticks(void);

#endif /* DRIVERS_TIMER_H */
