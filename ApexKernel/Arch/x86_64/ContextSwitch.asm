; apex-kernel/arch/x86_64/context_switch.asm
;
; ApexOS -- Context switch COOPERATIF (pas celui utilise par le timer).
;
; IMPORTANT -- pourquoi ce fichier n'est actuellement appele par rien :
; le switch declenche par le timer (PIT/IRQ0) est fait INLINE dans irq0
; (voir idt.asm), pas via context_switch() ici. Ce n'est PAS une erreur
; ni du code mort par accident -- ce sont deux mecanismes different par
; nature :
;
;   - irq0 (idt.asm)   : switch PREEMPTIF, declenche par une interruption
;                         materielle asynchrone. Au moment ou irq0 tourne,
;                         le CPU a deja pousse un frame d'interruption
;                         (RIP/CS/RFLAGS/RSP/SS du code interrompu) sur la
;                         pile, et irq0 a deja sauve tous les registres
;                         generaux a la main. Le layout memoire a ce
;                         moment-la ne correspond a RIEN de ce que
;                         context_switch() ci-dessous sait lire/ecrire.
;
;   - context_switch() : switch COOPERATIF, pense pour etre appelee comme
;                         une fonction C normale (`call context_switch`)
;                         depuis du code qui cede volontairement le CPU
;                         (futur yield()/sleep()/blocage sur I/O). Elle
;                         sauve son PROPRE RSP/RIP d'appelant -- ce qui
;                         n'a pas de sens dans le contexte d'une IRQ.
;
; Essayer de fusionner les deux (faire appeler context_switch() depuis
; irq0) demanderait de lui apprendre a comprendre un frame d'interruption,
; ce qui la rendrait quasi identique au code deja dans irq0 -- donc pas
; de vrai gain, et un risque de casser un mecanisme deja stable. Les deux
; fichiers restent donc volontairement separes.
;
; Cette fonction est prete a etre branchee des qu'on ajoute un vrai appel
; volontaire type yield()/sleep() dans le scheduler.

[BITS 64]

global context_switch
global proc_iretq_start

%define OFF_R15     0x00
%define OFF_R14     0x08
%define OFF_R13     0x10
%define OFF_R12     0x18
%define OFF_R11     0x20
%define OFF_R10     0x28
%define OFF_R9      0x30
%define OFF_R8      0x38
%define OFF_RBP     0x40
%define OFF_RDI     0x48
%define OFF_RSI     0x50
%define OFF_RDX     0x58
%define OFF_RCX     0x60
%define OFF_RBX     0x68
%define OFF_RAX     0x70
%define OFF_RIP     0x78
%define OFF_CS      0x80
%define OFF_RFLAGS  0x88
%define OFF_RSP     0x90
%define OFF_SS      0x98

section .bss
align 16
switch_stack_bottom:
    resb 4096
switch_stack_top:

section .text

; context_switch(cpu_context_t *old_ctx, cpu_context_t *new_ctx)
context_switch:
    ; ── Sauvegarde sur stack temporaire ──────────────────────────────────
    ; Sauvegarde RSP actuel et bascule sur switch_stack
    mov [rdi + OFF_RSP], rsp
    mov rsp, switch_stack_top

    ; Sauvegarde RIP = adresse de retour
    mov rax, [rdi + OFF_RSP]    ; ancien RSP
    mov rax, [rax]              ; valeur à cet RSP = return address
    mov [rdi + OFF_RIP], rax

    ; Sauvegarde registres généraux
    mov [rdi + OFF_R15], r15
    mov [rdi + OFF_R14], r14
    mov [rdi + OFF_R13], r13
    mov [rdi + OFF_R12], r12
    mov [rdi + OFF_R11], r11
    mov [rdi + OFF_R10], r10
    mov [rdi + OFF_R9],  r9
    mov [rdi + OFF_R8],  r8
    mov [rdi + OFF_RBP], rbp
    mov [rdi + OFF_RDX], rdx
    mov [rdi + OFF_RCX], rcx
    mov [rdi + OFF_RBX], rbx
    mov [rdi + OFF_RAX], rax

    ; Sauvegarde RDI et RSI
    mov [rdi + OFF_RDI], rdi
    mov [rdi + OFF_RSI], rsi

    ; Sauvegarde RFLAGS
    pushfq
    pop rax
    mov [rdi + OFF_RFLAGS], rax

    ; ── Restaure new_ctx ──────────────────────────────────────────────────
    mov r15, [rsi + OFF_R15]
    mov r14, [rsi + OFF_R14]
    mov r13, [rsi + OFF_R13]
    mov r12, [rsi + OFF_R12]
    mov r11, [rsi + OFF_R11]
    mov r10, [rsi + OFF_R10]
    mov r9,  [rsi + OFF_R9]
    mov r8,  [rsi + OFF_R8]
    mov rbp, [rsi + OFF_RBP]
    mov rdx, [rsi + OFF_RDX]
    mov rcx, [rsi + OFF_RCX]
    mov rbx, [rsi + OFF_RBX]
    mov rax, [rsi + OFF_RAX]

    ; Restaure RSP du nouveau processus
    mov rsp, [rsi + OFF_RSP]

    ; Push RIP du nouveau processus
    push qword [rsi + OFF_RIP]

    ; Restaure RDI et RSI en dernier
    mov rdi, [rsi + OFF_RDI]
    mov rsi, [rsi + OFF_RSI]

    ret

; ── proc_iretq_start ─────────────────────────────────────────────────────
proc_iretq_start:
    push qword [rdi + OFF_SS]
    push qword [rdi + OFF_RSP]
    push qword [rdi + OFF_RFLAGS]
    push qword [rdi + OFF_CS]
    push qword [rdi + OFF_RIP]

    mov r15, [rdi + OFF_R15]
    mov r14, [rdi + OFF_R14]
    mov r13, [rdi + OFF_R13]
    mov r12, [rdi + OFF_R12]
    mov r11, [rdi + OFF_R11]
    mov r10, [rdi + OFF_R10]
    mov r9,  [rdi + OFF_R9]
    mov r8,  [rdi + OFF_R8]
    mov rbp, [rdi + OFF_RBP]
    mov rdx, [rdi + OFF_RDX]
    mov rcx, [rdi + OFF_RCX]
    mov rbx, [rdi + OFF_RBX]
    mov rax, [rdi + OFF_RAX]
    mov rsi, [rdi + OFF_RSI]
    mov rdi, [rdi + OFF_RDI]

    iretq