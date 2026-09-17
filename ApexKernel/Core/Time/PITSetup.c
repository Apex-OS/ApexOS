// ApexKernel/Core/Time/PITSetup.c
// ApexOS -- PIT 8253 timer bootstrap

#include "Core/Time/PITSetup.h"
#include "Core/Time/PITCounter.h"
#include "Bus/PortIO.h"

void pit_init(void)
{
    uint16_t divisor = PIT_BASE_FREQ / PIT_TARGET_FREQ;

    // Mode 3: square wave generator, channel 0.
    outb(PIT_CMD, 0x36);
    outb(PIT_CHANNEL0, divisor & 0xFF);
    outb(PIT_CHANNEL0, (divisor >> 8) & 0xFF);

    pit_mark_ready();
}
