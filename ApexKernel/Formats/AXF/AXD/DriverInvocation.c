// ApexKernel/Formats/AXF/AXD/DriverInvocation.c
// ApexOS -- AXD exported function invocation

#include "Formats/AXF/AXD/DriverInvocation.h"
#include "Formats/AXF/AXD/HeaderValidation.h"

uint64_t axd_call(int driver_id, int func_id, uint64_t arg)
{
    if (driver_id < 0 || driver_id >= axd_driver_count)
        return (uint64_t)-1;

    axd_driver_t *drv = &loaded_drivers[driver_id];
    if (!drv->active)
        return (uint64_t)-1;

    if (func_id < 0 || (uint32_t)func_id >= drv->export_count)
        return (uint64_t)-1;

    uint64_t (*fn)(uint64_t) = (uint64_t (*)(uint64_t))drv->export_offsets[func_id];
    return fn(arg);
}
