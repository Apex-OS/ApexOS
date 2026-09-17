// ApexKernel/Includes/Boot/UEFI/Efi.h
// ApexOS -- Minimal UEFI type definitions (hand-written from the UEFI
// specification, no gnu-efi or other third-party UEFI library --
// consistent with this project's from-scratch approach: see
// ApexKernel/Boot/BIOS/ for the equivalent hand-written BIOS path).
//
// Only defines what ApexOS's UEFI bootloader actually uses -- this is
// not a general-purpose EFI header. Extended as the bootloader grows
// (memory map retrieval, file I/O, graphics -- see this file's
// companion .c for what's implemented so far).
//
// Compiled with x86_64-w64-mingw32-gcc (a real PE32+-targeting cross
// compiler, not plain gcc -- see the project's Makefile for the
// separate UEFI build rules). UEFI uses the Microsoft x64 calling
// convention (first 4 args in RCX/RDX/R8/R9, caller-allocated 32-byte
// shadow space) rather than System V (the rest of this kernel's ABI,
// via plain gcc) -- mingw-w64-gcc's default ABI already matches this,
// so no per-function calling-convention attributes are needed as long
// as every UEFI source file is built with the mingw compiler, never
// mixed into a System V object file.

#ifndef APEX_BOOT_UEFI_EFI_H
#define APEX_BOOT_UEFI_EFI_H

// Freestanding -- no libc, and this predates ApexLib's own
// Core/Types.h being usable here (that header assumes System V
// System-V-built code links against it; this is a separate PE32+
// build with its own toolchain, see this file's header comment). So:
// fixed-width types defined locally rather than shared with the rest
// of the kernel.
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;
typedef signed long long int64_t;
typedef uint64_t size_t;

typedef uint64_t EFI_STATUS;
typedef void *EFI_HANDLE;
typedef void *EFI_EVENT;
typedef uint16_t CHAR16;
typedef uint8_t BOOLEAN;
#define TRUE 1
#define FALSE 0
#define NULL ((void *)0)

// High bit set = error (EFI_STATUS is a signed-magnitude-style code:
// bit 63 set means failure, the low bits are the specific code) --
// see the UEFI spec's Appendix D "Status Codes". Only the two this
// bootloader actually checks for are defined; add more as needed
// rather than transcribing the whole appendix speculatively.
#define EFI_SUCCESS 0
#define EFI_ERROR_BIT (1ULL << 63)
#define EFI_LOAD_ERROR (EFI_ERROR_BIT | 1)
#define EFI_BUFFER_TOO_SMALL (EFI_ERROR_BIT | 5)
#define EFI_NOT_FOUND (EFI_ERROR_BIT | 14)

// EFI_LOCATE_SEARCH_TYPE -- only ByProtocol is used by this
// bootloader (LocateHandleBuffer(ByProtocol, &guid, ...) to enumerate
// every handle supporting a given protocol, e.g. every
// EFI_BLOCK_IO_PROTOCOL handle -- see Main.c's GPT-reading code).
#define ByProtocol 2

// EFI_GUID -- UEFI identifies protocols/interfaces by 128-bit GUIDs
// rather than a fixed function table, since firmware vendors can
// extend it. Struct layout must match the spec exactly (this is
// wire/ABI format, not just an internal convenience type).
typedef struct
{
    uint32_t data1;
    uint16_t data2;
    uint16_t data3;
    uint8_t data4[8];
} EFI_GUID;

// ── Simple Text Output Protocol (ConOut) ────────────────────────────
// The only protocol this Hello World needs -- console text output.
// Every EFI protocol struct is a table of function pointers the
// firmware fills in; the UEFI calling convention comment at the top
// of this file applies to every one of them.
typedef struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL;

typedef EFI_STATUS (*EFI_TEXT_STRING)(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *this, CHAR16 *string);
typedef EFI_STATUS (*EFI_TEXT_CLEAR_SCREEN)(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *this);
typedef EFI_STATUS (*EFI_TEXT_RESET)(EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *this, BOOLEAN extended_verification);

struct EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL
{
    EFI_TEXT_RESET Reset;
    EFI_TEXT_STRING OutputString;
    void *TestString;   // unused fields left as void* placeholders -- keeps struct offsets
    void *QueryMode;    // correct for the fields after them (OutputString is the only one
    void *SetMode;      // this bootloader currently calls) without transcribing every field's
    void *SetAttribute; // real function pointer type until it's actually needed.
    EFI_TEXT_CLEAR_SCREEN ClearScreen;
    void *SetCursorPosition;
    void *EnableCursor;
    void *Mode;
};

// ── EFI_TABLE_HEADER ─────────────────────────────────────────────────
typedef struct
{
    uint64_t signature;
    uint32_t revision;
    uint32_t header_size;
    uint32_t crc32;
    uint32_t reserved;
} EFI_TABLE_HEADER;

// ── EFI_BOOT_SERVICES (subset) ──────────────────────────────────────
// Only the members this bootloader calls so far are given real
// function pointer types; everything before/after them is void*
// padding to keep later members at their correct struct offset (same
// approach as EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL above) -- the real
// struct has ~40 members, transcribing all of them before they're
// needed would just be more surface area to get wrong.
typedef EFI_STATUS (*EFI_GET_MEMORY_MAP)(size_t *memory_map_size, void *memory_map,
                                         size_t *map_key, size_t *descriptor_size,
                                         uint32_t *descriptor_version);
typedef EFI_STATUS (*EFI_EXIT_BOOT_SERVICES)(EFI_HANDLE image_handle, size_t map_key);
typedef EFI_STATUS (*EFI_LOCATE_PROTOCOL)(EFI_GUID *protocol, void *registration, void **interface);
typedef EFI_STATUS (*EFI_ALLOCATE_POOL)(uint32_t pool_type, size_t size, void **buffer);
typedef EFI_STATUS (*EFI_FREE_POOL)(void *buffer);

typedef EFI_STATUS (*EFI_LOCATE_HANDLE_BUFFER)(uint32_t search_type, EFI_GUID *protocol,
                                               void *search_key, size_t *no_handles, EFI_HANDLE **buffer);
typedef EFI_STATUS (*EFI_HANDLE_PROTOCOL)(EFI_HANDLE handle, EFI_GUID *protocol, void **interface);

typedef struct
{
    EFI_TABLE_HEADER hdr;
    void *RaiseTPL;
    void *RestoreTPL;
    void *AllocatePages;
    void *FreePages;
    EFI_GET_MEMORY_MAP GetMemoryMap;
    EFI_ALLOCATE_POOL AllocatePool;
    EFI_FREE_POOL FreePool;
    void *CreateEvent;
    void *SetTimer;
    void *WaitForEvent;
    void *SignalEvent;
    void *CloseEvent;
    void *CheckEvent;
    void *InstallProtocolInterface;
    void *ReinstallProtocolInterface;
    void *UninstallProtocolInterface;
    EFI_HANDLE_PROTOCOL HandleProtocol;
    void *Reserved;
    void *RegisterProtocolNotify;
    void *LocateHandle;
    void *LocateDevicePath;
    void *InstallConfigurationTable;
    void *LoadImage;
    void *StartImage;
    void *Exit;
    void *UnloadImage;
    EFI_EXIT_BOOT_SERVICES ExitBootServices;
    void *GetNextMonotonicCount;
    void *Stall;
    void *SetWatchdogTimer;
    void *ConnectController;
    void *DisconnectController;
    void *OpenProtocol;
    void *CloseProtocol;
    void *OpenProtocolInformation;
    void *ProtocolsPerHandle;
    EFI_LOCATE_HANDLE_BUFFER LocateHandleBuffer;
    EFI_LOCATE_PROTOCOL LocateProtocol;
    void *InstallMultipleProtocolInterfaces;
    void *UninstallMultipleProtocolInterfaces;
    void *CalculateCrc32;
    void *CopyMem;
    void *SetMem;
    void *CreateEventEx;
} EFI_BOOT_SERVICES;

// ── EFI_CONFIGURATION_TABLE ──────────────────────────────────────────
// system_table->configuration_table (below) points to an array of
// number_of_table_entries of these -- firmware's way of publishing
// "here's where table X lives in memory" for tables that aren't a
// callable protocol (ACPI's RSDP chief among them: this bootloader
// needs it to hand the kernel a route to MADT for SMP CPU discovery,
// see Boot/UEFI/AcpiFind.c). Matching a known table by GUID means
// walking this array and comparing vendor_guid against the constants
// below.
typedef struct
{
    EFI_GUID vendor_guid;
    void *vendor_table;
} EFI_CONFIGURATION_TABLE;

// ACPI_TABLE_GUID (ACPI 1.0 RSDP) -- {eb9d2d30-2d88-11d3-9a16-0090273fc14a}
#define EFI_ACPI_1_0_TABLE_GUID \
    {0xeb9d2d30, 0x2d88, 0x11d3, {0x9a, 0x16, 0x00, 0x90, 0x27, 0x3f, 0xc1, 0x4a}}

// ACPI_20_TABLE_GUID (ACPI 2.0+ RSDP, has the XSDT pointer) --
// {8868e871-e4f1-11d3-bc22-0080c73c8881}. Preferred over the 1.0 GUID
// when both are present -- see AcpiFind.c for the search order.
#define EFI_ACPI_2_0_TABLE_GUID \
    {0x8868e871, 0xe4f1, 0x11d3, {0xbc, 0x22, 0x00, 0x80, 0xc7, 0x3c, 0x88, 0x81}}

// ── EFI_SYSTEM_TABLE (subset) ───────────────────────────────────────
typedef struct
{
    EFI_TABLE_HEADER hdr;
    CHAR16 *firmware_vendor;
    uint32_t firmware_revision;
    EFI_HANDLE console_in_handle;
    void *con_in;
    EFI_HANDLE console_out_handle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *con_out;
    EFI_HANDLE standard_error_handle;
    EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *std_err;
    void *runtime_services;
    EFI_BOOT_SERVICES *boot_services;
    size_t number_of_table_entries;
    EFI_CONFIGURATION_TABLE *configuration_table;
} EFI_SYSTEM_TABLE;

// ── Graphics Output Protocol (GOP) ──────────────────────────────────
// UEFI's equivalent of BIOS VBE (see ApexKernel/Boot/BIOS/Transition/
// HardwareDetect.asm for that path) -- gives a linear framebuffer
// address plus resolution/pitch/pixel-format, without needing mode
// numbers or real-mode BIOS calls the way VBE does. Located via
// LocateProtocol() with its well-known GUID (EFI_GRAPHICS_OUTPUT_
// PROTOCOL_GUID below) rather than being a System Table member --
// see this file's header comment on EFI_BOOT_SERVICES.LocateProtocol.
typedef struct
{
    uint32_t version;
    uint32_t horizontal_resolution;
    uint32_t vertical_resolution;
    uint32_t pixel_format;         // EFI_GRAPHICS_PIXEL_FORMAT -- see the PixelFormat constants below
    uint32_t pixel_bitmask[4];     // only meaningful when pixel_format == PixelBitMask; unused for
                                   // the RGB/BGR formats this bootloader actually targets (see Main.c)
    uint32_t pixels_per_scan_line; // pitch, in pixels -- may exceed horizontal_resolution (padding),
                                   // same convention as this kernel's existing fb_boot_info_t.pitch
} EFI_GRAPHICS_OUTPUT_MODE_INFORMATION;

// EFI_GRAPHICS_PIXEL_FORMAT enum values (UEFI spec, Section 11.9) --
// PixelBltOnly and PixelFormatMax deliberately omitted, this
// bootloader only ever accepts the first two (a linear 32bpp
// framebuffer it can write to directly) and rejects anything else --
// see Main.c.
#define PixelRedGreenBlueReserved8BitPerColor 0
#define PixelBlueGreenRedReserved8BitPerColor 1
#define PixelBitMask 2
#define PixelBltOnly 3

typedef struct
{
    uint32_t max_mode;
    uint32_t mode;
    EFI_GRAPHICS_OUTPUT_MODE_INFORMATION *info;
    size_t size_of_info;
    uint64_t frame_buffer_base;
    size_t frame_buffer_size;
} EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE;

typedef struct EFI_GRAPHICS_OUTPUT_PROTOCOL EFI_GRAPHICS_OUTPUT_PROTOCOL;

typedef EFI_STATUS (*EFI_GRAPHICS_OUTPUT_PROTOCOL_QUERY_MODE)(
    EFI_GRAPHICS_OUTPUT_PROTOCOL *this, uint32_t mode_number,
    size_t *size_of_info, EFI_GRAPHICS_OUTPUT_MODE_INFORMATION **info);
typedef EFI_STATUS (*EFI_GRAPHICS_OUTPUT_PROTOCOL_SET_MODE)(
    EFI_GRAPHICS_OUTPUT_PROTOCOL *this, uint32_t mode_number);

struct EFI_GRAPHICS_OUTPUT_PROTOCOL
{
    EFI_GRAPHICS_OUTPUT_PROTOCOL_QUERY_MODE QueryMode;
    EFI_GRAPHICS_OUTPUT_PROTOCOL_SET_MODE SetMode;
    void *Blt; // unused -- this bootloader writes the framebuffer directly (see fb_boot_info_t's
               // whole point: a linear buffer the kernel keeps writing to after boot), not through
               // GOP's blit-rectangle call, so left as a void* placeholder like Efi.h's other
               // not-yet-needed protocol members.
    EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE *Mode;
};

// EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID, from the UEFI spec -- identifies
// GOP for LocateProtocol(). Every UEFI protocol has one of these;
// they're fixed by the spec, not something a bootloader invents.
#define EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID \
    {0x9042a9de, 0x23dc, 0x4a38, {0x96, 0xfb, 0x7a, 0xde, 0xd0, 0x80, 0x51, 0x6a}}

// ── EFI_LOADED_IMAGE_PROTOCOL ───────────────────────────────────────
// Automatically installed on this bootloader's own image_handle (the
// first efi_main() parameter) -- its DeviceHandle identifies which
// device/partition this .efi was actually loaded from (the ESP), and
// is the handle Main.c passes to HandleProtocol() below to get to the
// Simple File System Protocol for that same partition, to read
// kernel.bin off it. Only the fields Main.c actually reads are kept
// as real types; the rest are void* padding, same convention as this
// file's other protocol structs.
typedef struct
{
    uint32_t revision;
    EFI_HANDLE parent_handle;
    EFI_SYSTEM_TABLE *system_table;
    EFI_HANDLE device_handle;
    void *file_path;
    void *reserved;
    uint32_t load_options_size;
    void *load_options;
    void *image_base;
    uint64_t image_size;
    uint32_t image_code_type;
    uint32_t image_data_type;
    void *unload;
} EFI_LOADED_IMAGE_PROTOCOL;

#define EFI_LOADED_IMAGE_PROTOCOL_GUID \
    {0x5B1B31A1, 0x9562, 0x11d2, {0x8E, 0x3F, 0x00, 0xA0, 0xC9, 0x69, 0x72, 0x3B}}

// ── EFI_SIMPLE_FILE_SYSTEM_PROTOCOL / EFI_FILE_PROTOCOL ─────────────
// Read-only file access is all this bootloader needs (loading
// kernel.bin off the ESP) -- Write/Delete/Flush are kept as void*
// placeholders, same convention as this file's other not-yet-needed
// protocol members.
typedef struct EFI_FILE_PROTOCOL EFI_FILE_PROTOCOL;

typedef EFI_STATUS (*EFI_FILE_OPEN)(EFI_FILE_PROTOCOL *this, EFI_FILE_PROTOCOL **new_handle,
                                    CHAR16 *file_name, uint64_t open_mode, uint64_t attributes);
typedef EFI_STATUS (*EFI_FILE_CLOSE)(EFI_FILE_PROTOCOL *this);
typedef EFI_STATUS (*EFI_FILE_READ)(EFI_FILE_PROTOCOL *this, size_t *buffer_size, void *buffer);
typedef EFI_STATUS (*EFI_FILE_GET_INFO)(EFI_FILE_PROTOCOL *this, EFI_GUID *information_type,
                                        size_t *buffer_size, void *buffer);

struct EFI_FILE_PROTOCOL
{
    uint64_t revision;
    EFI_FILE_OPEN Open;
    EFI_FILE_CLOSE Close;
    void *Delete;
    EFI_FILE_READ Read;
    void *Write;
    void *GetPosition;
    void *SetPosition;
    EFI_FILE_GET_INFO GetInfo;
    void *SetInfo;
    void *Flush;
};

#define EFI_FILE_MODE_READ 0x0000000000000001ULL

// EFI_FILE_INFO -- what GetInfo() fills in when asked for
// EFI_FILE_INFO_ID (below). file_size is the only field Main.c
// actually needs (to know how large a buffer to allocate before
// Read()), so everything after it is left unread even though the real
// struct has a few more timestamp/attribute fields -- this local
// struct only needs to be large enough and correctly laid out for the
// fields it does name; GetInfo() writes the rest past file_size into
// this struct's tail regardless, which the caller-provided buffer
// (Main.c) sizes generously for (see that file).
typedef struct
{
    uint64_t size;
    uint64_t file_size;
    uint64_t physical_size;
    // created/last_access/modification timestamps and attribute
    // fields follow in the real struct -- omitted, see comment above.
} EFI_FILE_INFO;

#define EFI_FILE_INFO_ID \
    {0x09576e92, 0x6d3f, 0x11d2, {0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b}}

typedef struct
{
    uint64_t revision;
    EFI_STATUS (*OpenVolume)(void *this, EFI_FILE_PROTOCOL **root);
} EFI_SIMPLE_FILE_SYSTEM_PROTOCOL;

#define EFI_SIMPLE_FILE_SYSTEM_PROTOCOL_GUID \
    {0x0964e5b22, 0x6459, 0x11d2, {0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b}}

// ── EFI_MEMORY_DESCRIPTOR ───────────────────────────────────────────
// What GetMemoryMap() fills an array of -- used both to build the
// memory map ApexKernel's PMM needs (see Core/Memory/Physical/
// PhysicalMemoryDetection.c's existing E820-based path for BIOS; this
// is UEFI's equivalent source of the same information) and as the
// required map_key input to ExitBootServices() (the two calls must
// use the map from the same GetMemoryMap() call -- see Main.c).
//
// Explicit uint32_t pad after `type` rather than relying on the
// compiler's natural 8-byte alignment inserting it implicitly -- both
// give the same layout here, but writing it out removes any doubt
// this struct matches the firmware's actual memory layout regardless
// of struct-packing settings a future change to this file's build
// flags might introduce.
typedef struct
{
    uint32_t type; // EFI_MEMORY_TYPE -- see the EfiConventionalMemory comment below
    uint32_t pad;
    uint64_t physical_start;
    uint64_t virtual_start;
    uint64_t number_of_pages; // in 4KB pages, regardless of this bootloader's own page size assumptions
    uint64_t attribute;
} EFI_MEMORY_DESCRIPTOR;

// EFI_MEMORY_TYPE values this bootloader actually distinguishes --
// the full enum has ~15 values (loader code/data, boot/runtime
// services code/data, ACPI reclaim, MMIO, etc.); only the ones needed
// to tell "usable RAM" apart from everything else are named here,
// same reduced-surface-area approach as this file's protocol structs.
// EfiConventionalMemory is genuinely free RAM; EfiBootServicesCode/Data
// becomes free too once ExitBootServices() has run (the firmware no
// longer needs that memory), which matters for how Main.c reports
// usable memory to the kernel.
#define EfiLoaderCode 1
#define EfiLoaderData 2
#define EfiBootServicesCode 3
#define EfiBootServicesData 4
#define EfiConventionalMemory 7

// ── EFI_BLOCK_IO_PROTOCOL ───────────────────────────────────────────
// Raw block-level disk access -- needed to read the GPT header and
// partition entries directly (they're disk structures, not files a
// filesystem protocol like EFI_SIMPLE_FILE_SYSTEM_PROTOCOL, see
// above, would expose). Only ReadBlocks is typed; Reset/WriteBlocks/
// FlushBlocks are void* placeholders, same convention as this file's
// other protocol structs -- this bootloader only ever reads the GPT,
// never writes it.
typedef struct
{
    uint32_t media_id;
    BOOLEAN removable_media;
    BOOLEAN media_present;
    BOOLEAN logical_partition;
    BOOLEAN read_only;
    BOOLEAN write_caching;
    uint8_t _pad[3];
    uint32_t block_size;
    uint32_t io_align;
    uint64_t last_block;
} EFI_BLOCK_IO_MEDIA;

typedef struct EFI_BLOCK_IO_PROTOCOL EFI_BLOCK_IO_PROTOCOL;

typedef EFI_STATUS (*EFI_BLOCK_READ)(EFI_BLOCK_IO_PROTOCOL *this, uint32_t media_id,
                                     uint64_t lba, size_t buffer_size, void *buffer);

struct EFI_BLOCK_IO_PROTOCOL
{
    uint64_t revision;
    EFI_BLOCK_IO_MEDIA *media;
    void *Reset;
    EFI_BLOCK_READ ReadBlocks;
    void *WriteBlocks;
    void *FlushBlocks;
};

#define EFI_BLOCK_IO_PROTOCOL_GUID \
    {0x964e5b21, 0x6459, 0x11d2, {0x8e, 0x39, 0x00, 0xa0, 0xc9, 0x69, 0x72, 0x3b}}

// ── GPT (GUID Partition Table) ──────────────────────────────────────
// Read directly off the disk via EFI_BLOCK_IO_PROTOCOL (above) rather
// than through any UEFI partition-aware protocol -- this bootloader
// needs the *raw* partition layout (specifically: where the ESP ends,
// so @SysDisk/AXFS knows where it can safely start -- see
// Core/Startup/SysDiskSetup.c's previous hardcoded-2048 approach and
// why it collided with a real ESP) rather than firmware-mediated file
// access to any one partition.
//
// #pragma pack(1)-equivalent layout (GCC/mingw both honor
// __attribute__((packed)) the same way) -- this is wire format from
// the UEFI spec (Section 5.3.2/5.3.3), not negotiable struct padding.
typedef struct __attribute__((packed))
{
    uint64_t signature; // "EFI PART", see GPT_HEADER_SIGNATURE below
    uint32_t revision;
    uint32_t header_size;
    uint32_t header_crc32;
    uint32_t reserved;
    uint64_t my_lba;
    uint64_t alternate_lba;
    uint64_t first_usable_lba;
    uint64_t last_usable_lba;
    EFI_GUID disk_guid;
    uint64_t partition_entry_lba;
    uint32_t number_of_partition_entries;
    uint32_t size_of_partition_entry;
    uint32_t partition_entry_array_crc32;
} GPT_HEADER;

#define GPT_HEADER_SIGNATURE 0x5452415020494645ULL // "EFI PART" as a little-endian uint64_t

typedef struct __attribute__((packed))
{
    EFI_GUID partition_type_guid;
    EFI_GUID unique_partition_guid;
    uint64_t starting_lba;
    uint64_t ending_lba; // inclusive -- the partition occupies [starting_lba, ending_lba], see this
                         // file's Main.c for the +1/*block_size conversion to a byte-length/offset
    uint64_t attributes;
    CHAR16 partition_name[36];
} GPT_PARTITION_ENTRY;

#endif // APEX_BOOT_UEFI_EFI_H
