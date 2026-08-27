; apex-kernel/arch/x86_64/gdt.asm
; ApexOS — GDT flush + segment reload

[BITS 64]

global gdt_flush

gdt_flush:
    ; RDI = pointeur vers gdt_descriptor_t (convention SysV AMD64 ABI)
    lgdt [rdi]

    ; Recharge CS via un far return
    push 0x08                   ; Kernel code segment
    lea rax, [rel .reload_cs]
    push rax
    retfq                       ; Far return 64-bit → recharge CS

.reload_cs:
    ; Recharge les segments data
    mov ax, 0x10                ; Kernel data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    ret