// ApexKernel/Includes/Formats/AXF/AXP/HeaderValidation.h
// ApexOS -- AXP (.axp executable) header validation

#ifndef APEX_FORMATS_AXF_AXP_HEADER_VALIDATION_H
#define APEX_FORMATS_AXF_AXP_HEADER_VALIDATION_H

#include "Core/Types.h"

#include "Formats/AXF/AXFFormat.h"

// Validates the AXF header read for a .axp: magic, version, type ==
// AXF_TYPE_AXP, section count in range, and base_addr == the only
// address v1 supports (USER_CODE_ADDR). Returns false (and logs why)
// if invalid.
bool axp_validate_header(const axf_header_t *header);

#endif // APEX_FORMATS_AXF_AXP_HEADER_VALIDATION_H
