// ApexKernel/Includes/Core/Time/Uptime.h
// ApexOS -- Uptime formatting (ticks -> minutes/seconds)

#ifndef APEX_CORE_TIME_UPTIME_H
#define APEX_CORE_TIME_UPTIME_H

#include "Core/Types.h"

// Converts the current PIT tick count into minutes/seconds. Extracted
// out of the shell's cmd_uptime (see
// ApexKernel/Core/Shell/Commands/SystemCommands.c) so the time-unit
// math lives in Core/Time/, not in a shell command.
void time_get_uptime(uint64_t *out_mins, uint64_t *out_secs);

#endif // APEX_CORE_TIME_UPTIME_H
