; ApexKernel/Arch/x86_64/ApBootTrampoline.asm
; ApexOS -- AP wakeup trampoline (16-bit real mode -> 32-bit protected
; -> 64-bit long mode), see Core/SMP/Startup/AppProcessorStartup.h
;
; This code does NOT run from wherever the linker places it in
; kernel.elf. A SIPI always starts an AP executing in 16-bit real mode
; at physical address (vector << 12) -- AP_TRAMPOLINE_ADDR (0x8000,
; AppProcessorStartup.h) -- so ap_startup_begin() (AppProcessorStartup.c)
; memcpy()s the bytes between ap_trampoline_start and ap_trampoline_end
; (exported below) to that exact physical address before sending any
; SIPI. Every address referenced anywhere in this file is therefore a
; hardcoded absolute address of the form AP_TRAMPOLINE_ADDR + <offset
; within this file> -- NOT a normal linked/relocated address the way
; every other .asm file in this kernel uses -- because the only
; address this code will ever actually execute at is the one it's
; copied to, regardless of where NASM/the linker place these bytes
; within kernel.elf itself.
;
; Structurally this mirrors Boot/BIOS/Transition/LongModeEntry.asm's
; own 16->32->64 transition almost exactly (same PAE/EFER.LME/CR0
; sequence), with two differences: this code reuses the BSP's already-
; built KERNEL_PML4 (0x1000) instead of calling setup_paging again
; (see this file's ap_pm32_entry: below for why rebuilding page tables
; here would be actively dangerous), and it ends by jumping into C
; (ap_c_entry(), AppProcessorStartup.c) instead of directly into
; kmain() -- an AP has no independent boot sequence to run, everything
; it needs (GDT, IDT, page tables, heap) already exists, built by the
; BSP before this trampoline was ever copied into place.

%define AP_TRAMPOLINE_ADDR 0x8000
%define AP_TRAMPOLINE_DATA_OFFSET 0xF00
%define KERNEL_PML4 0x1000

global ap_trampoline_start
global ap_trampoline_end

section .text

[BITS 16]
ap_trampoline_start:
ap_real_mode_entry:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax

    ; No stack exists yet at this exact point (ap_trampoline_data_t's
    ; stack_top, filled in by the BSP, isn't loaded into rsp until
    ; ap_long_mode_entry: below, once this AP is in 64-bit mode) --
    ; none of this 16-bit section pushes/calls anything, purely
    ; straight-line code and direct jumps, so none is needed yet.

    lgdt [AP_TRAMPOLINE_ADDR + (ap_gdt32_descriptor - ap_trampoline_start)]

    mov eax, cr0
    or eax, 1 ; CR0.PE
    mov cr0, eax

    jmp 0x08:(AP_TRAMPOLINE_ADDR + (ap_pm32_entry - ap_trampoline_start))

; ── GDT 32-bit (flat code/data, identical shape to Boot/BIOS/
;    Transition/LongModeEntry.asm's own gdt32_start -- this trampoline
;    needs its own copy rather than reusing that one because it isn't
;    guaranteed to still be mapped/intact at whatever address it was
;    originally built at by the time an AP wakes, arbitrarily later in
;    boot) ──────────────────────────────────────────────────────────
align 8
ap_gdt32_start:
    dq 0
    dw 0xFFFF, 0x0000
    db 0x00, 10011010b, 11001111b, 0x00
    dw 0xFFFF, 0x0000
    db 0x00, 10010010b, 11001111b, 0x00
ap_gdt32_end:
ap_gdt32_descriptor:
    dw ap_gdt32_end - ap_gdt32_start - 1
    dd AP_TRAMPOLINE_ADDR + (ap_gdt32_start - ap_trampoline_start)

[BITS 32]
ap_pm32_entry:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    ; No ESP set here either -- still nothing on this AP's path pushes
    ; or calls before ap_long_mode_entry: loads the real stack_top
    ; from shared data, same reasoning as ap_real_mode_entry: above.

    ; Loads the 64-bit GDT NOW, while still in 32-bit protected mode --
    ; must happen before the far jump below, which targets selector
    ; 0x08 against THIS GDT (see ap_gdt64_start's own comment). An
    ; earlier version of this file never loaded this GDT at all,
    ; leaving the 32-bit GDT (only two entries, 0x08/0x10) active
    ; straight through the far jump below -- caught via -d int: a
    ; General Protection Fault at that far jump's own address,
    ; immediately cascading into a double fault since no IDT is loaded
    ; yet either to handle it.
    lgdt [AP_TRAMPOLINE_ADDR + (ap_gdt64_descriptor - ap_trampoline_start)]

    ; ── PAE ──────────────────────────────────────────────────────────
    mov eax, cr4
    or eax, (1 << 5)
    mov cr4, eax

    ; ── CR3 = the BSP's already-built KERNEL_PML4 ──────────────────────
    ; Deliberately NOT calling anything equivalent to
    ; LongModeEntry.asm's setup_paging here: that function
    ; unconditionally rebuilds PML4/PDPT/PD/PT from scratch at fixed
    ; addresses (0x1000-0x6000) shared by every CPU -- if a second AP
    ; is concurrently mid-boot through this same trampoline (see
    ; AppProcessorStartup.c's ap_startup_wake_one(): this kernel wakes
    ; APs one at a time specifically to avoid this), or if the BSP
    ; itself is still executing kernel code that depends on those
    ; tables staying intact, rebuilding them here would corrupt every
    ; other CPU's page tables out from under it. KERNEL_PML4 already
    ; has the complete identity map vmm_init() built (Core/Memory/
    ; Virtual/IdentityMap.c) long before any AP is ever woken --
    ; reusing it read-only (from this AP's perspective) is both
    ; correct and the only safe option.
    mov eax, KERNEL_PML4
    mov cr3, eax

    ; ── EFER.LME ─────────────────────────────────────────────────────
    mov ecx, 0xC0000080
    rdmsr
    or eax, (1 << 8)
    wrmsr

    ; ── PG + PE ──────────────────────────────────────────────────────
    mov eax, cr0
    or eax, (1 << 31) | 1
    mov cr0, eax

    jmp 0x08:(AP_TRAMPOLINE_ADDR + (ap_long_mode_entry - ap_trampoline_start))

; ── GDT 64-bit (identical shape to LongModeEntry.asm's gdt64_start --
;    same reasoning as ap_gdt32_start above for why this trampoline
;    keeps its own copy rather than reusing that one). Loaded via its
;    OWN lgdt just above, entirely replacing the 32-bit GDT rather
;    than extending it -- so this GDT's own selectors start over at
;    0x00 (null)/0x08 (code)/0x10 (data), same numbering as the 32-bit
;    GDT used, not a continuation of it. The far jump above targets
;    0x08 for exactly that reason (this GDT's own second entry, code),
;    not 0x18 -- an earlier version of this file used 0x18 here on the
;    mistaken assumption that both GDTs' entries shared one selector
;    space, producing a General Protection Fault the instant this far
;    jump executed (caught via -d int: v=0d right at this
;    instruction's own address, immediately cascading into a double
;    fault with no IDT loaded yet to handle either) ──────────────────
align 8
ap_gdt64_start:
    dq 0
    dw 0x0000, 0x0000
    db 0x00, 10011010b, 10101111b, 0x00
    dw 0x0000, 0x0000
    db 0x00, 10010010b, 00000000b, 0x00
ap_gdt64_end:
ap_gdt64_descriptor:
    dw ap_gdt64_end - ap_gdt64_start - 1
    dq AP_TRAMPOLINE_ADDR + (ap_gdt64_start - ap_trampoline_start)

[BITS 64]
ap_long_mode_entry:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Shared data (ap_trampoline_data_t, AppProcessorStartup.h) lives
    ; at a fixed offset within this same trampoline page -- the BSP
    ; writes stack_top/c_entry_addr/cpu_index there (Core/SMP/Startup/
    ; AppProcessorStartup.c's ap_startup_wake_one()) before this AP's
    ; SIPI was ever sent, so by the time execution reaches here those
    ; values are guaranteed already in place.
    mov rax, AP_TRAMPOLINE_ADDR + AP_TRAMPOLINE_DATA_OFFSET
    mov rsp, [rax + 0]      ; stack_top
    mov rbx, [rax + 8]      ; c_entry_addr
    mov edi, [rax + 16]     ; cpu_index -- first (and only) argument to ap_c_entry(), SysV AMD64 ABI

    ; rbp left at whatever garbage was in it -- ap_c_entry() (C code,
    ; standard prologue) sets up its own frame immediately, nothing
    ; here needs rbp to already be valid.
    call rbx

    ; ap_c_entry() is documented (AppProcessorStartup.c) to never
    ; return -- this is purely a safety net in case a future change to
    ; it accidentally falls through.
    cli
.halt:
    hlt
    jmp .halt

ap_trampoline_end: