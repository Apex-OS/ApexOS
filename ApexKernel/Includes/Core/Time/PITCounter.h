// ApexKernel/Includes/Core/Time/PITCounter.h
// ApexOS -- PIT 8253 tick counter

#ifndef APEX_CORE_TIME_PIT_COUNTER_H
#define APEX_CORE_TIME_PIT_COUNTER_H

#include "Core/Types.h"

#define PIT_CHANNEL0 0x40
#define PIT_CHANNEL1 0x41
#define PIT_CHANNEL2 0x42
#define PIT_CMD 0x43
#define PIT_BASE_FREQ 1193182 // Hz
#define PIT_TARGET_FREQ 100   // 100Hz -> tick every 10ms

uint64_t pit_get_ticks(void);

// Called from TimerInterrupt.asm on every IRQ0.
void pit_tick(void);

// True once pit_init() (PITSetup.c) has programmed the PIT and IRQ0 is
// unmasked, i.e. once pit_get_ticks() reflects real elapsed time
// rather than a counter that's never being incremented. Used by
// Logs/Log.c's boot-log timestamp: before this is true, elapsed time
// simply isn't known yet, so every log line prints "0.000" rather
// than a real-looking value derived from a counter that hasn't
// started -- same convention dmesg itself follows (timestamps read
// 0.000000 until the kernel's own clock is initialized).
bool pit_is_ready(void);

// Called by pit_init() once it's done programming the PIT -- not
// meant to be called from anywhere else.
void pit_mark_ready(void);

#endif // APEX_CORE_TIME_PIT_COUNTER_H
