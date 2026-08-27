// ApexKernel/Includes/Core/Panic.h
// ApexOS -- Kernel panic

#ifndef APEX_CORE_PANIC_H
#define APEX_CORE_PANIC_H

// Prints `msg` in white-on-red, then halts the CPU permanently. Never
// returns.
void kpanic(const char *msg);

#endif // APEX_CORE_PANIC_H
