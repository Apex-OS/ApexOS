// ApexKernel/Core/Time/Uptime.c
// ApexOS -- Uptime formatting (ticks -> minutes/seconds)

#include "Core/Time/Uptime.h"
#include "Core/Time/PITCounter.h"

void time_get_uptime(uint64_t *out_mins, uint64_t *out_secs)
{
    uint64_t ticks = pit_get_ticks();
    uint64_t secs = ticks / 100;
    *out_mins = secs / 60;
    *out_secs = secs % 60;
}
