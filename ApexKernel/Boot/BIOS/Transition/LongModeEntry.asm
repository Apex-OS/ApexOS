; ApexKernel/Boot/BIOS/Transition/LongModeEntry.asm
; ApexOS -- Protected mode transition, paging setup, and Long Mode entry

[BITS 16]

PML4_ADDR           equ 0x1000
PDPT_ADDR           equ 0x2000
PD_ADDR             equ 0x3000
PT_ADDR             equ 0x4000

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