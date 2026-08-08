// apex-kernel/drivers/pit.c
// ApexOS -- PIT 8253 timer driver

#include "pit.h"
#include "../include/apex/io.h"

static volatile uint64_t pit_ticks = 0;

void pit_init(void)
{
    uint16_t divisor = PIT_BASE_FREQ / PIT_TARGET_FREQ;

    // Mode 3: square wave generator, channel 0.
    outb(PIT_CMD, 0x36);
    outb(PIT_CHANNEL0, divisor & 0xFF);
    outb(PIT_CHANNEL0, (divisor >> 8) & 0xFF);
}

uint64_t pit_get_ticks(void)
{
    return pit_ticks;
}

void pit_tick(void)
{
    pit_ticks++;
}