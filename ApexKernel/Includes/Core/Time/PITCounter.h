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

#endif // APEX_CORE_TIME_PIT_COUNTER_H
