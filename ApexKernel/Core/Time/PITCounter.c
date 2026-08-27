// ApexKernel/Core/Time/PITCounter.c
// ApexOS -- PIT 8253 tick counter

#include "Core/Time/PITCounter.h"

static volatile uint64_t pit_ticks = 0;

uint64_t pit_get_ticks(void)
{
    return pit_ticks;
}

void pit_tick(void)
{
    pit_ticks++;
}
