; apex-kernel/arch/x86_64/boot/stage2.asm
; ApexOS Stage 2 Bootloader — FINAL
; Détection mémoire E820 + Long mode 64-bit

[BITS 16]
[ORG 0x7E00]

; ── Constantes ───────────────────────────────────────────────────────────────
KERNEL_TMP_SEG      equ 0x1000
KERNEL_TMP_OFF      equ 0x0000
KERNEL_FINAL_ADDR   equ 0x100000
KERNEL_SECTORS      equ 128
KERNEL_LBA          equ 18

MMAP_ADDR           equ 0x500
MMAP_COUNT          equ 0x4FC

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
    jmp enter_protected_mode

; ── Enable A20 ───────────────────────────────────────────────────────────────
enable_a20:
    in al, 0x92
    or al, 2
    out 0x92, al
    ret

; ── Charge kernel à 0x10000 ──────────────────────────────────────────────────
load_kernel_low:
    mov si, msg_loading
    call print16

    mov ah, 0x42
    mov dl, [boot_drive]
    mov si, dap
    int 0x13
    jc .error

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
dap:
    db 0x10
    db 0
    dw KERNEL_SECTORS
    dw KERNEL_TMP_OFF
    dw KERNEL_TMP_SEG
    dq KERNEL_LBA

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