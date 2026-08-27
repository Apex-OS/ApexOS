// ApexKernel/Includes/Formats/AXF/AXD/DriverInvocation.h
// ApexOS -- AXD exported function invocation

#ifndef APEX_FORMATS_AXF_AXD_DRIVER_INVOCATION_H
#define APEX_FORMATS_AXF_AXD_DRIVER_INVOCATION_H

#include "Core/Types.h"

// Calls exported function number `func_id` (0-based index into the
// export table, NOT "driver_init" which is already called at load
// time) of driver `driver_id`, with a single generic argument.
// Returns the function's return value, or (uint64_t)-1 if
// driver_id/func_id is invalid or the driver is inactive.
uint64_t axd_call(int driver_id, int func_id, uint64_t arg);

#endif // APEX_FORMATS_AXF_AXD_DRIVER_INVOCATION_H
