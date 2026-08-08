; apex-kernel/arch/x86_64/boot/stage2.asm
; ApexOS Stage 2 Bootloader — FINAL
; Détection mémoire E820 + Long mode 64-bit

[BITS 16]
[ORG 0x7E00]

; ── Constantes ───────────────────────────────────────────────────────────────
KERNEL_TMP_SEG      equ 0x1000
KERNEL_TMP_OFF      equ 0x0000
KERNEL_FINAL_ADDR   equ 0x100000
KERNEL_SECTORS      equ 160        ; BUG FIX: was 128 (64KB) -- kernel.bin
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

; ── Enable A20 ───────────────────────────────────────────────────────────────
enable_a20:
    in al, 0x92
    or al, 2
    out 0x92, al
    ret

; ── Charge kernel à 0x10000 ──────────────────────────────────────────────────
; Loads the kernel in multiple chunks, each well under the 64KB
; per-read limit some BIOSes enforce on the destination segment (the
; single-DAP-call approach used here before broke once KERNEL_SECTORS
; grew to 160 (80KB): "offset + 512*count - 1" exceeded 0xFFFF, and
; SeaBIOS (QEMU's default BIOS) refused the read outright with "Disk
; read error!"). CHUNK_SECTORS=64 (32KB) stays safely under that limit
; for every chunk, with the destination segment advanced by 32KB
; (0x800 paragraphs) between reads -- KERNEL_TMP_SEG itself never
; moves, only dap_buffer_seg (a copy used per-chunk) does, so
; KERNEL_TMP_SEG stays the fixed base pm32_entry's copy loop expects.
CHUNK_SECTORS equ 64

load_kernel_low:
    mov si, msg_loading
    call print16

    mov word [dap_buffer_seg], KERNEL_TMP_SEG
    mov dword [dap_lba_lo], KERNEL_LBA
    mov word [remaining_sectors], KERNEL_SECTORS

.chunk_loop:
    cmp word [remaining_sectors], 0
    je .done

    ; This chunk's size: min(CHUNK_SECTORS, remaining).
    mov ax, [remaining_sectors]
    cmp ax, CHUNK_SECTORS
    jbe .size_ok
    mov ax, CHUNK_SECTORS
.size_ok:
    mov [dap_count], ax

    mov ah, 0x42
    mov dl, [boot_drive]
    mov si, dap
    int 0x13
    jc .error

    ; Advances by this chunk's size: LBA += count, segment += count*32
    ; (count*512 bytes / 16 bytes-per-paragraph = count*32).
    movzx eax, word [dap_count]
    add dword [dap_lba_lo], eax
    sub [remaining_sectors], ax

    shl ax, 5 ; count * 32
    add [dap_buffer_seg], ax

    jmp .chunk_loop

.done:
    mov si, msg_loaded
    call print16
    ret

.error:
    mov si, msg_disk_err
    call print16
    cli
    hlt

; ── Détection mémoire E820 ───────────────────────────────────────────────────
detect_memory:
    mov ax, 0x0000
    mov es, ax
    mov di, MMAP_ADDR
    xor ebx, ebx
    xor bp, bp
    mov edx, 0x534D4150

.next_entry:
    mov eax, 0xE820
    mov ecx, 24
    int 0x15
    jc .done

    cmp eax, 0x534D4150
    jne .done

    test ecx, ecx
    jz .skip

    inc bp
    add di, 24

.skip:
    test ebx, ebx
    jz .done
    jmp .next_entry

.done:
    mov word [MMAP_COUNT], bp
    ret

; ── VBE mode detection ───────────────────────────────────────────────────────
; Queries the BIOS for every supported VESA mode, picks the largest
; 32bpp graphics mode with a linear framebuffer, sets it, and writes a
; small summary struct to VBE_RESULT_ADDR for the kernel to read (see
; drivers/framebuffer.h -- must match this layout exactly):
;   +0  uint32_t framebuffer_addr
;   +4  uint16_t width
;   +6  uint16_t height
;   +8  uint16_t pitch        (bytes per scanline)
;   +10 uint8_t  bpp
;   +11 uint8_t  valid        (0 = no suitable mode found, VGA text mode stays active)
;
; Leaves VGA text mode untouched if no suitable mode is found (`valid`
; stays 0) -- the kernel decides what to do with that, this bootloader
; doesn't force graphics mode on an unwilling/unsupported machine.
detect_vbe_mode:
    push es

    ; Zeroes the result struct first -- if anything below bails out
    ; early, `valid` is already 0 and the kernel correctly sees "no
    ; graphics mode available".
    mov di, VBE_RESULT_ADDR
    mov cx, 12
    xor al, al
    rep stosb

    ; Zeroes the VBE info buffer too -- BUG FIX: this was left
    ; uninitialized before, and only the 4-byte 'VBE2' signature was
    ; written into it. If the BIOS doesn't fill the ENTIRE 512-byte
    ; buffer it promises to (or fills less than expected under this
    ; particular BIOS/QEMU version), VideoModePtr at offset 0x0E could
    ; be read from garbage leftover memory instead of a real BIOS
    ; value -- this was the root cause of the corrupted mode numbers
    ; (0xB595, 0xFF53F000 "addresses" that were actually leftover
    ; ROM/stack garbage) seen during debugging.
    mov ax, 0x0000
    mov es, ax
    mov di, VBE_INFO_ADDR
    mov cx, 512
    xor al, al
    rep stosb

    ; ── Function 4F00h: Get VBE Controller Info ───────────────────────────
    mov ax, 0x0000
    mov es, ax
    mov di, VBE_INFO_ADDR
    mov dword [es:di], 'VBE2' ; requests VBE 2.0+ fields (extended, e.g. 32bpp modes)
    mov ax, 0x4F00
    int 0x10
    cmp ax, 0x004F
    jne .no_vbe ; BIOS doesn't support VBE at all -- stays in VGA text mode

    ; VideoModePtr (offset 0x0E, far pointer segment:offset) -> the
    ; list of supported mode numbers, terminated by 0xFFFF.
    mov si, [VBE_INFO_ADDR + 0x10] ; offset
    mov ax, [VBE_INFO_ADDR + 0x0E + 2] ; segment
    mov [mode_list_seg], ax ; saved in memory -- fs is NOT guaranteed to
                             ; survive an int 0x10 call, so it gets
                             ; reloaded from here on every loop
                             ; iteration below, rather than trusted to
                             ; still hold this value after any BIOS call.
    mov fs, ax

    ; Best candidate found so far (0xFFFF = none yet).
    mov word [best_mode], 0xFFFF
    mov word [best_width], 0
    mov word [best_height], 0

.scan_modes:
    ; Reloads fs every iteration -- see the comment by mode_list_seg's
    ; save above.
    mov ax, [mode_list_seg]
    mov fs, ax

    mov cx, fs:[si]
    cmp cx, 0xFFFF
    je .scan_done
    add si, 2

    ; Saves the mode number in a fixed memory location right away,
    ; BEFORE the int 0x10 call below -- fs is not guaranteed to
    ; survive a BIOS interrupt call (very few BIOSes explicitly
    ; preserve it), so re-reading fs:[si-2] AFTER int 0x10 (like the
    ; original version of this loop did) risked reading garbage from
    ; whatever fs now pointed at instead of the real mode list. This
    ; was the cause of the "mode 0xB595" corruption seen in early
    ; testing.
    mov [current_mode], cx
    push si

    ; ── Function 4F01h: Get Mode Info for this candidate ──────────────────
    mov ax, 0x0000
    mov es, ax
    mov di, VBE_MODE_INFO_ADDR
    mov ax, 0x4F01
    int 0x10
    cmp ax, 0x004F
    jne .next_mode ; some listed modes can be invalid -- BIOS said so, skip it

    ; attributes (offset 0x00): bit 0 = supported, bit 3 = color, bit 4
    ; = graphics (not text), bit 7 = linear framebuffer available.
    mov ax, [VBE_MODE_INFO_ADDR + 0x00]
    test ax, 0x0001
    jz .next_mode
    test ax, 0x0008
    jz .next_mode
    test ax, 0x0010
    jz .next_mode
    test ax, 0x0080
    jz .next_mode

    ; Only interested in 32bpp (offset 0x19) -- keeps the pixel format
    ; simple and predictable for the framebuffer driver (4 bytes/pixel,
    ; no 24bpp packing edge cases to handle).
    mov al, [VBE_MODE_INFO_ADDR + 0x19]
    cmp al, 32
    jne .next_mode

    ; Keeps this mode if its width*height is bigger than the best seen
    ; so far. Compares width first, then height, rather than the true
    ; product (avoids a 32-bit multiply in 16-bit real mode for a
    ; comparison that doesn't need to be exact -- picking the largest
    ; width is enough to get a good mode in practice).
    mov ax, [VBE_MODE_INFO_ADDR + 0x12] ; width
    cmp ax, [best_width]
    jb .next_mode
    ja .better
    mov dx, [VBE_MODE_INFO_ADDR + 0x14] ; height (tie-break on width)
    cmp dx, [best_height]
    jbe .next_mode

.better:
    ; Reads the mode number from current_mode (saved in memory before
    ; the BIOS call above), NOT from fs:[si-2] -- see the comment by
    ; current_mode's save above for why.
    mov cx, [current_mode]
    mov [best_mode], cx
    mov ax, [VBE_MODE_INFO_ADDR + 0x12]
    mov [best_width], ax
    mov ax, [VBE_MODE_INFO_ADDR + 0x14]
    mov [best_height], ax

.next_mode:
    pop si
    jmp .scan_modes

.scan_done:
    cmp word [best_mode], 0xFFFF
    je .no_vbe ; scanned everything, nothing suitable -- stays in VGA text mode

    ; ── Re-fetch Mode Info for the winning mode (needed fields: pitch,
    ; PhysBasePtr -- discarded during the scan above to keep the loop
    ; simple) ──────────────────────────────────────────────────────────────
    mov ax, 0x0000
    mov es, ax
    mov di, VBE_MODE_INFO_ADDR
    mov cx, [best_mode]
    mov ax, 0x4F01
    int 0x10
    cmp ax, 0x004F
    jne .no_vbe

    ; ── Function 4F02h: Set VBE Mode ───────────────────────────────────────
    ; Bit 14 (0x4000) requests a linear framebuffer -- already confirmed
    ; supported by this mode's attributes bit 7 during the scan.
    mov cx, [best_mode]
    or cx, 0x4000
    mov bx, cx
    mov ax, 0x4F02
    int 0x10
    cmp ax, 0x004F
    jne .no_vbe

    ; ── Writes the result struct for the kernel ────────────────────────────
    mov eax, [VBE_MODE_INFO_ADDR + 0x28] ; PhysBasePtr
    mov [VBE_RESULT_ADDR + 0], eax
    mov ax, [VBE_MODE_INFO_ADDR + 0x12] ; width
    mov [VBE_RESULT_ADDR + 4], ax
    mov ax, [VBE_MODE_INFO_ADDR + 0x14] ; height
    mov [VBE_RESULT_ADDR + 6], ax
    mov ax, [VBE_MODE_INFO_ADDR + 0x10] ; bytes per scanline
    mov [VBE_RESULT_ADDR + 8], ax
    mov al, [VBE_MODE_INFO_ADDR + 0x19] ; bits per pixel
    mov [VBE_RESULT_ADDR + 10], al
    mov byte [VBE_RESULT_ADDR + 11], 1  ; valid = true

    pop es
    ret

.no_vbe:
    ; VBE_RESULT_ADDR was already zeroed at the top -- `valid` stays 0,
    ; nothing more to do. VGA text mode (already active) is left as-is.
    pop es
    ret

best_mode      dw 0
best_width     dw 0
best_height    dw 0
current_mode   dw 0
mode_list_seg  dw 0

; ── Print 16-bit ─────────────────────────────────────────────────────────────
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
; DAP fields are named individually so load_kernel_low's chunk loop
; can update count/segment/LBA between reads (see dap_count,
; dap_buffer_seg, dap_lba_lo/dap_lba_hi below -- same memory, just
; labeled per-field instead of one opaque block).
dap:
    db 0x10
    db 0
dap_count:
    dw KERNEL_SECTORS ; overwritten per-chunk by load_kernel_low
dap_buffer_off:
    dw KERNEL_TMP_OFF
dap_buffer_seg:
    dw KERNEL_TMP_SEG ; overwritten per-chunk by load_kernel_low
dap_lba_lo:
    dd KERNEL_LBA      ; overwritten per-chunk by load_kernel_low
dap_lba_hi:
    dd 0

remaining_sectors: dw 0

boot_drive  db 0

msg_stage2      db "ApexOS Stage2 OK", 0x0D, 0x0A, 0
msg_loading     db "Loading kernel...", 0x0D, 0x0A, 0
msg_loaded      db "Kernel loaded OK!", 0x0D, 0x0A, 0
msg_disk_err    db "Disk read error!", 0x0D, 0x0A, 0

; ── Passage en mode protégé ───────────────────────────────────────────────────
enter_protected_mode:
    cli
    lgdt [gdt32_descriptor]
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    jmp 0x08:pm32_entry

; ── GDT 32-bit ───────────────────────────────────────────────────────────────
align 8
gdt32_start:
    dq 0
    dw 0xFFFF, 0x0000
    db 0x00, 10011010b, 11001111b, 0x00
    dw 0xFFFF, 0x0000
    db 0x00, 10010010b, 11001111b, 0x00
gdt32_end:
gdt32_descriptor:
    dw gdt32_end - gdt32_start - 1
    dd gdt32_start

; ════════════════════════════════════════════════════════════════════════════
[BITS 32]
pm32_entry:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000

    ; ── Copie kernel 0x10000 → 0x100000 ─────────────────────────────────────
    mov esi, 0x10000
    mov edi, 0x100000
    mov ecx, (KERNEL_SECTORS * 512) / 4
    rep movsd

    ; ── Setup paging ─────────────────────────────────────────────────────────
    call setup_paging

    ; ── GDT 64-bit ───────────────────────────────────────────────────────────
    lgdt [gdt64_descriptor]

    ; ── PAE ──────────────────────────────────────────────────────────────────
    mov eax, cr4
    or eax, (1 << 5)
    mov cr4, eax

    ; ── CR3 ──────────────────────────────────────────────────────────────────
    mov eax, PML4_ADDR
    mov cr3, eax

    ; ── EFER LME ─────────────────────────────────────────────────────────────
    mov ecx, 0xC0000080
    rdmsr
    or eax, (1 << 8)
    wrmsr

    ; ── PG + PE ──────────────────────────────────────────────────────────────
    mov eax, cr0
    or eax, (1 << 31) | 1
    mov cr0, eax

    jmp 0x08:lm64_entry

; ── Setup paging 0→4MB ───────────────────────────────────────────────────────
setup_paging:
    mov edi, PML4_ADDR
    mov ecx, 0x5000 / 4
    xor eax, eax
    rep stosd

    mov dword [PML4_ADDR], PDPT_ADDR | 0x03
    mov dword [PDPT_ADDR], PD_ADDR   | 0x03

    mov dword [PD_ADDR + 0], PT_ADDR          | 0x03
    mov dword [PD_ADDR + 8], (PT_ADDR + 0x1000) | 0x03

    ; PT1 : 0x0 → 0x200000
    mov edi, PT_ADDR
    mov eax, 0x03
    mov ecx, 512
.fill1:
    mov [edi], eax
    add eax, 0x1000
    add edi, 8
    loop .fill1

    ; PT2 : 0x200000 → 0x400000
    mov ecx, 512
.fill2:
    mov [edi], eax
    add eax, 0x1000
    add edi, 8
    loop .fill2

    ret

; ── GDT 64-bit ───────────────────────────────────────────────────────────────
align 8
gdt64_start:
    dq 0
    dw 0x0000, 0x0000
    db 0x00, 10011010b, 10101111b, 0x00
    dw 0x0000, 0x0000
    db 0x00, 10010010b, 00000000b, 0x00
gdt64_end:
gdt64_descriptor:
    dw gdt64_end - gdt64_start - 1
    dq gdt64_start

; ════════════════════════════════════════════════════════════════════════════
[BITS 64]
lm64_entry:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov rsp, 0x1F0000

    mov rax, 0x100000
    call rax

    cli
.halt:
    hlt
    jmp .halt