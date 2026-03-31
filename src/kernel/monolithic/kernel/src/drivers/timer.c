#include <stdint.h>

#include <drivers/timer.h>
#include <kernel/io.h>
#include <kernel/log.h>

static volatile uint32_t pit_ticks = 0;

void timer_init(uint32_t frequency)
{
    if (frequency == 0)
    {
        return;
    }

    uint32_t divisor = 1193180 / frequency;

    // Command byte: channel 0, access mode lobyte/hibyte, mode 3 (square wave), binary mode
    outb(0x43, 0x36);
    outb(0x40, (unsigned char)(divisor & 0xFF));
    outb(0x40, (unsigned char)((divisor >> 8) & 0xFF));

    printk(LOG_INFO, "PIT: Initialized at %u Hz (divisor=%u)", frequency, divisor);
}

void timer_handle_interrupt(void)
{
    pit_ticks++;
}

uint32_t timer_get_ticks(void)
{
    return pit_ticks;
}
