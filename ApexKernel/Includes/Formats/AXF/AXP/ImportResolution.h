// ApexKernel/Includes/Formats/AXF/AXP/ImportResolution.h
// ApexOS -- AXP import table resolution (loads needed .axl libraries)

#ifndef APEX_FORMATS_AXF_AXP_IMPORT_RESOLUTION_H
#define APEX_FORMATS_AXF_AXP_IMPORT_RESOLUTION_H

#include "Core/Types.h"

#include "Formats/AXF/AXFFormat.h"
#include "Filesystem/AXFS/AXFS.h"

// Resolves and writes every import in the .axp's import table (see
// axf_header_t.import_table_offset) into the fixed
// AXF_IMPORT_TABLE_ADDR page the program reads at startup. `filebuf`
// is the raw file bytes already read by axf_load_and_run(); `vol`/
// `dir_inode` are where to look up the imported .axl files. Returns
// false on any failure (missing library/function, too many imports,
// out of memory) -- ProcessLaunch.c's axf_load_and_run() bails out
// without having touched the .axp's own memory yet in that case.
bool axp_resolve_imports(axfs_volume_t *vol, uint64_t dir_inode, const axf_header_t *header,
                         const uint8_t *filebuf, uint64_t read_bytes);

#endif // APEX_FORMATS_AXF_AXP_IMPORT_RESOLUTION_H
