// ApexKernel/Includes/Formats/AXF/AXD/ExportResolution.h
// ApexOS -- AXD export table resolution (internal to axd_load)

#ifndef APEX_FORMATS_AXF_AXD_EXPORT_RESOLUTION_H
#define APEX_FORMATS_AXF_AXD_EXPORT_RESOLUTION_H

#include "Formats/AXF/AXD/HeaderValidation.h"
#include "Formats/AXF/AXFFormat.h"

// Resolves every export in `filebuf`'s export table into `drv`
// (absolute addresses, base_addr already applied), and returns the
// resolved address of "driver_init" specifically (0 if not found --
// axd_load() treats that as a load failure).
uint64_t axd_resolve_exports(const axf_header_t *header, const uint8_t *filebuf,
                             uint64_t read_bytes, axd_driver_t *drv);

#endif // APEX_FORMATS_AXF_AXD_EXPORT_RESOLUTION_H
