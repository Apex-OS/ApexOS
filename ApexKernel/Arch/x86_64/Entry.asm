; apex-kernel/arch/x86_64/entry.asm
; ApexOS — Kernel entry point (doit être le PREMIER dans .text)

[BITS 64]

global _start
extern kmain

section .text
_start:
    ; Nettoie les registres
    xor rbp, rbp

    ; Appelle kmain
    call kmain

    ; Si kmain retourne — ne devrait jamais arriver
    cli
.halt:
    hlt
    jmp .halt