; ApexKernel/Boot/BIOS/Transition/KernelLoader.asm
; ApexOS -- A20 line enable + kernel image load (chunked DAP reads)

[BITS 16]

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

