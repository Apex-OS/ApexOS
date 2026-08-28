; ApexKernel/Boot/BIOS/Stage2.asm
; ApexOS Stage 2 Bootloader -- orchestrator
; Detection memoire E820 + Long mode 64-bit
;
; Constants and shared data (DAP, boot_drive, messages) live here since
; they're used across multiple files below (KernelLoader.asm,
; HardwareDetect.asm, LongModeEntry.asm). print16 lives here too since
; every stage2 file uses it for status messages.

[BITS 16]
[ORG 0x7E00]

; ── Constantes ───────────────────────────────────────────────────────────────
KERNEL_TMP_SEG      equ 0x1000
KERNEL_TMP_OFF      equ 0x0000
KERNEL_FINAL_ADDR   equ 0x100000
KERNEL_SECTORS      equ 200        ; Grown from 160 to fit ahci.c/pci.c --
                                    ; kernel.bin (82768 bytes as of this
                                    ; change) exceeded 160 sectors (81920
                                    ; bytes), truncating the loaded kernel
                                    ; and causing a silent hang before the
                                    ; first boot log (confirmed: build
                                    ; succeeded, but nothing appeared on
                                    ; serial even though TSS init is the
                                    ; very first thing kmain() logs).
                                    ; 200 sectors = 100KB, ~19KB headroom
                                    ; above the kernel's current size.
                                    ; grew past that (66368 bytes, needs 130
                                    ; sectors minimum) once the framebuffer
                                    ; driver and VBE-related code were added.
                                    ; The tail of the kernel binary -- which
                                    ; happens to hold the .data section,
                                    ; including vmm.c's `pml4` static
                                    ; variable -- was silently left
                                    ; uncopied/unloaded, leaving pml4 at 0
                                    ; instead of KERNEL_PML4 (0x1000). This
                                    ; caused a GPF in vmm_get_or_create()
                                    ; dereferencing a NULL page table during
                                    ; vmm_init(). 160 sectors (80KB) leaves
                                    ; headroom for further growth.
KERNEL_LBA          equ 34 ; BUG FIX: was 18, but the Makefile now
                            ; writes the kernel starting at sector 34
                            ; (stage2 grew from 16 to 32 sectors when
                            ; VBE detection code was added -- see
                            ; STAGE2_SECTORS in stage1.asm and the
                            ; dd seek= values in the Makefile). This
                            ; stale LBA was loading kernel.bin from
                            ; the wrong disk offset -- actually still
                            ; inside stage2's own sectors -- producing
                            ; a corrupted, partially-garbage "kernel"
                            ; in memory. This was the real cause of
                            ; the early gdt_flush/tss_init hang seen
                            ; while debugging VBE mode detection: the
                            ; hang was unrelated to VBE, and present
                            ; even with VBE detection fully disabled.

MMAP_ADDR           equ 0x9600 ; BUG FIX: was 0x500 -- something (the BIOS
                                ; itself, during int 0x10 VBE calls, most
                                ; likely) was overwriting this low-memory
                                ; zone with garbage during VBE mode
                                ; detection, corrupting the E820 map before
                                ; the kernel ever read it (pmm_init() then
                                ; read a bogus entry count in the hundreds
                                ; of millions and crashed). Moved well away
                                ; from every other low-memory structure this
                                ; bootloader uses, and clear of the VBE
                                ; buffers too (VBE_INFO_ADDR 0x9000-0x9200,
                                ; VBE_MODE_INFO_ADDR 0x9200-0x9300,
                                ; VBE_RESULT_ADDR 0x9300-0x930C).
MMAP_COUNT          equ 0x95FC ; kept in the 512 bytes just before MMAP_ADDR,
                                ; same relative layout as before (grep
                                ; MMAP_COUNT_ADDR in include/apex/mmap.h --
                                ; must match this exactly)

; VBE buffers -- placed well clear of MMAP_ADDR/PML4_ADDR/the temporary
; kernel load address (0x10000), see the comment above vbe_info_block.
VBE_INFO_ADDR       equ 0x9000 ; 512-byte VbeInfoBlock (function 4F00h)
VBE_MODE_INFO_ADDR  equ 0x9200 ; 256-byte ModeInfoBlock, reused per candidate mode (function 4F01h)
VBE_RESULT_ADDR     equ 0x9300 ; final chosen-mode summary, read by the kernel (see drivers/framebuffer.h)

; Upper bound on the resolution detect_vbe_mode will pick -- see the
; comment at the width/height rejection check below for why (kernel
; back-buffer sizing). 1600x900 chosen to match the resolution already
; in use -- HEAP_MAX_SIZE (mm/heap.h) was grown to 8MB specifically to
; fit this size back-buffer (1600*900*4 = ~5.76MB) with headroom.
MAX_FB_WIDTH        equ 1600
MAX_FB_HEIGHT       equ 900

PML4_ADDR           equ 0x1000
PDPT_ADDR           equ 0x2000
PD_ADDR             equ 0x3000
PT_ADDR             equ 0x4000

; ── Entry point ──────────────────────────────────────────────────────────────
stage2_start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti

    mov [boot_drive], dl

    mov si, msg_stage2
    call print16

    call enable_a20
    call load_kernel_low
    call detect_memory
    call detect_vbe_mode
    jmp enter_protected_mode

print16:
    mov ah, 0x0E
.loop:
    lodsb
    test al, al
    jz .done
    int 0x10
    jmp .loop
.done:
    ret

; ── DAP ──────────────────────────────────────────────────────────────────────
dap:
    db 0x10
    db 0
dap_count:
    dw KERNEL_SECTORS
dap_buffer_off:
    dw KERNEL_TMP_OFF
dap_buffer_seg:
    dw KERNEL_TMP_SEG
dap_lba_lo:
    dd KERNEL_LBA
dap_lba_hi:
    dd 0

remaining_sectors: dw 0

boot_drive  db 0

msg_stage2      db "ApexOS Stage2 OK", 0x0D, 0x0A, 0
msg_loading     db "Loading kernel...", 0x0D, 0x0A, 0
msg_loaded      db "Kernel loaded OK!", 0x0D, 0x0A, 0
msg_disk_err    db "Disk read error!", 0x0D, 0x0A, 0

; ── Included stage2 files ─────────────────────────────────────────────────
%include "Transition/KernelLoader.asm"
%include "Transition/HardwareDetect.asm"
%include "Transition/LongModeEntry.asm"
