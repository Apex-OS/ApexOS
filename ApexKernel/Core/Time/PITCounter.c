// ApexKernel/Core/Time/PITCounter.c
// ApexOS -- PIT 8253 tick counter

#include "Core/Time/PITCounter.h"

static volatile uint64_t pit_ticks = 0;
static volatile bool pit_ready = false;

uint64_t pit_get_ticks(void)
{
    return pit_ticks;
}

void pit_tick(void)
{
    pit_ticks++;
}

bool pit_is_ready(void)
{
    return pit_ready;
}

void pit_mark_ready(void)
{
    pit_ready = true;
}
