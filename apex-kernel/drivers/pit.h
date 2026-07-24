// apex-kernel/drivers/pit.h
// ApexOS -- PIT 8253 timer driver

#ifndef APEX_PIT_H
#define APEX_PIT_H

#include "../include/apex/types.h"

#define PIT_CHANNEL0 0x40
#define PIT_CHANNEL1 0x41
#define PIT_CHANNEL2 0x42
#define PIT_CMD 0x43
#define PIT_BASE_FREQ 1193182 // Hz
#define PIT_TARGET_FREQ 100   // 100Hz -> tick every 10ms

void pit_init(void);
uint64_t pit_get_ticks(void);
void pit_tick(void); // Called by IRQ0

#endif // APEX_PIT_H