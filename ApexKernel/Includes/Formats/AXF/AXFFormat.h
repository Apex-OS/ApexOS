// ApexKernel/Includes/Formats/AXF/AXFFormat.h
// ApexOS -- AXF (Apex eXecutable Format) v1
//
// Binary format for ApexOS executables (.axp), libraries (.axl), and
// kernel drivers (.axd) -- same base AXF format, distinguished by the
// header's `type` field.
//
// This header is SHARED between:
//   - the kernel (Formats/AXF/AXD/, Formats/AXF/AXP/), which READS
//     this format to load a binary into memory and run it
//   - the future userspace toolchain (ApexToolchain/), which will
//     WRITE this format as output
//
// No logic here, only the format's structure -- no dependency on any
// kernel- or userspace-specific code.
//
// V1 -- deliberately simple: a single base address, fixed-permission
// sections (.text exec, .data/.bss read-write), no relocations yet.

#ifndef APEX_FORMATS_AXF_FORMAT_H
#define APEX_FORMATS_AXF_FORMAT_H

// This header must compile both on the kernel side (freestanding,
// -nostdinc, no <stdint.h> available) and on the standard userspace
// toolchain side (with a real libc). AXF_HAVE_STDINT can be defined
// by the caller BEFORE including this file to force the use of
// <stdint.h> (e.g. from a tool running on the host system with a full
// libc). By default, a freestanding environment is assumed (like the
// rest of the ApexOS kernel) and the minimal types are defined here.
#ifdef AXF_HAVE_STDINT
#include <stdint.h>
#elif !defined(APEX_CORE_TYPES_H)
// Core/Types.h (kernel) isn't already included -- define the minimal
// types ourselves. If Core/Types.h IS already included before this
// header (the normal kernel case: the AXF loader includes
// "Core/Types.h" first), nothing is redefined here -- the
// APEX_CORE_TYPES_H guard acts as a shared guard against double
// definition.
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;
#endif

#define AXF_MAGIC 0x21465841u // "AXF!" little-endian on disk
#define AXF_VERSION 1

// Default virtual base address where a .axp is loaded (consistent
// with USER_CODE_ADDR, already used by `ring3` -- see
// Core/Userspace.h).
#define AXF_DEFAULT_BASE_ADDR 0x400000ull

#define AXF_MAX_SECTION_NAME 15

typedef enum
{
    AXF_TYPE_AXP = 1, // Apex Program -- standalone executable
    AXF_TYPE_AXL = 2, // Apex Library -- shared library, loaded into a process's own address space
    AXF_TYPE_AXD = 3, // Apex Driver -- kernel driver, loaded kernel-only (Ring 0)
} axf_type_t;

// Section flags (memory permissions).
#define AXF_SECTION_READ (1u << 0)
#define AXF_SECTION_WRITE (1u << 1)
#define AXF_SECTION_EXEC (1u << 2)

// File header (first block of the file).
typedef struct __attribute__((packed))
{
    uint32_t magic;   // AXF_MAGIC
    uint16_t version; // AXF_VERSION
    uint16_t type;    // axf_type_t

    uint64_t entry_point; // virtual address of the entry point (in .text)
                          // For .axl/.axd: ignored by the loader -- a
                          // library or driver isn't "run" directly,
                          // only mapped; its functions are called
                          // through the export table below.
    uint64_t base_addr;   // desired virtual base address (0 = AXF_DEFAULT_BASE_ADDR)

    uint32_t section_count;
    uint32_t section_table_offset; // file offset to the axf_section_t array

    // Export table -- used by .axl (functions the library offers to
    // .axp files that load it) and .axd the same way (e.g. exporting
    // "driver_init"). export_count = 0 for a plain .axp with no
    // exports (the normal case, also true for every file generated
    // before these two fields existed -- they land in the old
    // "reserved" space, already zeroed).
    uint32_t export_count;
    uint32_t export_table_offset; // file offset to the axf_export_t array

    // Import table -- used by .axp (and later .axl depending on
    // another .axl) to declare functions this file needs from an
    // external .axl. import_count = 0 for a standalone binary with no
    // dependency (true of every .axp that existed before these two
    // fields were added -- they land in the old "reserved" space,
    // already zeroed).
    uint32_t import_count;
    uint32_t import_table_offset; // file offset to the axf_import_t array

    uint64_t reserved[2]; // headroom for future extensions (relocations, etc.)
} axf_header_t;

// One section describes a memory range to map at load time: either
// copied from the file (.text, .data), or just allocated and
// zero-filled with no on-disk data (.bss -- file_offset/file_size are
// 0).
typedef struct __attribute__((packed))
{
    char name[AXF_MAX_SECTION_NAME + 1]; // e.g. ".text", ".data", ".bss" (null-terminated)

    uint64_t vaddr;       // virtual address to map this section at
    uint64_t mem_size;    // in-memory size (can exceed file_size for .bss or padding)
    uint64_t file_offset; // offset in the AXF file where the data starts (0 for .bss)
    uint64_t file_size;   // on-disk data size to copy (0 for .bss)

    uint32_t flags; // combination of AXF_SECTION_READ/WRITE/EXEC
    uint32_t _pad0;
} axf_section_t;

#define AXF_MAX_EXPORT_NAME 31

// Describes a function exposed by this file (typically a .axl or
// .axd) that other binaries can call after loading it. `offset` is
// relative to the start of the section containing the code
// (typically .text) -- the loader computes the real absolute address
// by adding the base address the file was actually mapped at
// (vaddr + offset), not a hardcoded address from the file.
typedef struct __attribute__((packed))
{
    char name[AXF_MAX_EXPORT_NAME + 1]; // e.g. "add" (null-terminated)
    uint64_t offset;                    // offset from the start of .text
} axf_export_t;

#define AXF_MAX_LIB_NAME 31

// Describes a function this file needs from an external .axl.
// `lib_name` is the library's file name (e.g. "libmath.axl"), looked
// up in the same directory as the importing binary (V1 -- no real
// system search path like System/Libraries/AXL/ yet, will come
// later).
typedef struct __attribute__((packed))
{
    char lib_name[AXF_MAX_LIB_NAME + 1];     // e.g. "libmath.axl"
    char func_name[AXF_MAX_EXPORT_NAME + 1]; // e.g. "add"
} axf_import_t;

// Resolved import table (in memory, not in the file). Once the needed
// .axl files are loaded and the imported functions' addresses
// computed, the loader writes those addresses HERE, in the order they
// appear in the .axp's import table. The .axp then just reads these
// uint64_t values in order to know the real addresses to call (same
// principle as a simplified PE IAT or ELF GOT).
//
// Address chosen right after a .axp's code/data area (USER_CODE_ADDR
// + 4096, see Core/Userspace.h) -- still a fixed constant in V1, no
// real dynamic management of multiple memory regions per process yet.
#define AXF_IMPORT_TABLE_ADDR 0x401000ull

#endif // APEX_FORMATS_AXF_FORMAT_H
