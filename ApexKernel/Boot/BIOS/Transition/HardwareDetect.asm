; ApexKernel/Boot/BIOS/Transition/HardwareDetect.asm
; ApexOS -- E820 memory map detection + VBE graphics mode detection

[BITS 16]

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

    ; Rejects any mode wider than MAX_FB_WIDTH or taller than
    ; MAX_FB_HEIGHT before comparing it as a candidate -- the kernel's
    ; graphics back-buffer (drivers/framebuffer_draw.c, double
    ; buffering) is allocated with kmalloc() out of a 4MB heap
    ; (mm/heap.h), so an uncapped "pick the biggest available" mode
    ; could pick a resolution whose back-buffer (width*height*4 bytes)
    ; doesn't fit. 1024x768x4 = 3MB, safely inside the heap alongside
    ; other allocations.
    mov ax, [VBE_MODE_INFO_ADDR + 0x12] ; width
    cmp ax, MAX_FB_WIDTH
    ja .next_mode
    mov ax, [VBE_MODE_INFO_ADDR + 0x14] ; height
    cmp ax, MAX_FB_HEIGHT
    ja .next_mode

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
