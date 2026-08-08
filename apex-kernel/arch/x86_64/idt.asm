; apex-kernel/arch/x86_64/idt.asm
; ApexOS — IDT + Context switch dans IRQ0

[BITS 64]

global idt_flush
global isr0,  isr1,  isr2,  isr3,  isr4,  isr5,  isr6,  isr7
global isr8,  isr9,  isr10, isr11, isr12, isr13, isr14, isr15
global isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23
global isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31
global irq0,  irq1,  irq2,  irq3,  irq4,  irq5,  irq6,  irq7
global irq8,  irq9,  irq10, irq11, irq12, irq13, irq14, irq15

extern interrupt_handler
extern scheduler_tick_asm   ; Notre nouveau handler ASM pour IRQ0

section .text

idt_flush:
    lidt [rdi]
    ret

; ── Macros ───────────────────────────────────────────────────────────────
%macro ISR_NOERR 1
isr%1:
    push qword 0
    push qword %1
    jmp isr_common
%endmacro

%macro ISR_ERR 1
isr%1:
    push qword %1
    jmp isr_common
%endmacro

%macro IRQ 2
irq%1:
    push qword 0
    push qword %2
    jmp irq_common
%endmacro

; ── ISR definitions ───────────────────────────────────────────────────────
ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR   8
ISR_NOERR 9
ISR_ERR   10
ISR_ERR   11
ISR_ERR   12
ISR_ERR   13
ISR_ERR   14
ISR_NOERR 15
ISR_NOERR 16
ISR_ERR   17
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_NOERR 21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_NOERR 29
ISR_NOERR 30
ISR_NOERR 31

; ── IRQ0 — Timer + context switch ────────────────────────────────────────
; IRQ0 est géré séparément pour le context switch
extern g_old_ctx
extern g_new_ctx
extern in_syscall
extern pit_tick

irq0:
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    ; ── Detection du CPL au moment de l'interruption ────────────────────
    ; Le CPU pousse automatiquement RIP/CS/RFLAGS (3 valeurs, 24 octets)
    ; SANS changement de privilege, ou RIP/CS/RFLAGS/RSP/SS (5 valeurs,
    ; 40 octets) AVEC changement de privilege (Ring3 -> Ring0). Le code
    ; ci-dessous (calcul d'offsets fixes type [rbp+128]=CS, [rbp+152]=SS)
    ; suppose TOUJOURS le cas a 5 valeurs.
    ;
    ; AVANT ce fix : si le PIT interrompt du code deja en Ring 0 (ex: le
    ; shell qui tourne apres un retour de `ring3` -- voir shell.c/
    ; syscall.c), le frame ne fait que 3 valeurs. Les offsets fixes
    ; lisaient/ecrivaient alors n'importe ou en memoire (le "SS" lu etait
    ; en fait une valeur random plus loin sur la pile), ce qui finissait
    ; par charger SS=0 (null selector) au iretq final -- #GP garanti des
    ; que le CPU essaie de toucher la pile avec ce SS invalide (typiquement
    ; a la prochaine interruption).
    ;
    ; Le CS est a un offset FIXE par rapport a RSP juste apres les 15
    ; push ci-dessus (120 octets), peu importe la taille du frame CPU (3
    ; ou 5 valeurs) : le CPU a pousse RIP puis CS juste avant qu'on entre
    ; ici, donc [rsp+0]=RIP, [rsp+8]=CS. On lit le RPL (2 bits de poids
    ; faible du selecteur) pour savoir d'ou on vient.
    mov rax, [rsp + 128]       ; CS pousse par le CPU (juste apres RIP)
    and rax, 0x3             ; RPL = bits [1:0] du selecteur
    cmp rax, 0x3
    je .from_ring3

    ; ── Cas Ring0 -> Ring0 (frame a 3 valeurs) ──────────────────────────
    ; Pas de switch de contexte possible/pertinent ici en pratique (le
    ; shell tourne toujours en Ring 0 -- meme SANS jamais passer par
    ; `ring3`, donc ce chemin est emprunte en permanence, pas seulement
    ; apres un retour de userspace). On appelle scheduler_tick_asm()
    ; (et PAS pit_tick() seul comme dans une version precedente de ce
    ; fix) car elle incremente aussi current_proc->ticks -- l'appeler
    ; uniquement via pit_tick() ici cassait le comptage de ticks du
    ; process courant en permanence (repere via le test kernel "current
    ; process ticks > 0" qui echouait meme sans jamais lancer `ring3`).
    ; scheduler_tick_asm() gere deja proprement le cas proc_count < 2
    ; (retourne sans tenter de switch, g_old_ctx/g_new_ctx restent NULL).
    ;
    ; On ignore delibermement g_old_ctx/g_new_ctx ici : meme si un futur
    ; scheduler_tick_asm() decidait qu'un switch est possible alors qu'on
    ; est dans ce chemin Ring0->Ring0 (ne devrait pas arriver tant que le
    ; scheduler ne gere que Ring3, mais par prudence), on ne fait PAS le
    ; switch de contexte ici car le frame CPU (3 valeurs) ne correspond
    ; pas a ce que le code de switch (.from_ring3 plus bas) attend.
    call scheduler_tick_asm
    mov al, 0x20
    out 0x20, al
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    iretq

.from_ring3:
    ; ── Cas Ring3 -> Ring0 (frame a 5 valeurs, comportement original) ───
    ; Sauvegarde RSP
    mov rbp, rsp

    call scheduler_tick_asm

    ; Restaure RSP
    mov rsp, rbp

    ; Charge old_ctx et new_ctx depuis les globales
    mov rax, [rel g_old_ctx]
    mov rdx, [rel g_new_ctx]

    ; Verifie in_syscall -- pas de switch si dans un syscall
    mov rcx, [rel in_syscall]
    test rcx, rcx
    jnz .eoi                ; Si in_syscall != 0, skip le switch

    test rax, rax
    jz .eoi
    test rdx, rdx
    jz .eoi
    cmp rax, rdx
    je .eoi

    ; ── Sauvegarde old_ctx ────────────────────────────────────────────────
    mov rcx, [rbp + 0]
    mov [rax + 0x00], rcx       ; r15
    mov rcx, [rbp + 8]
    mov [rax + 0x08], rcx       ; r14
    mov rcx, [rbp + 16]
    mov [rax + 0x10], rcx       ; r13
    mov rcx, [rbp + 24]
    mov [rax + 0x18], rcx       ; r12
    mov rcx, [rbp + 32]
    mov [rax + 0x20], rcx       ; r11
    mov rcx, [rbp + 40]
    mov [rax + 0x28], rcx       ; r10
    mov rcx, [rbp + 48]
    mov [rax + 0x30], rcx       ; r9
    mov rcx, [rbp + 56]
    mov [rax + 0x38], rcx       ; r8
    mov rcx, [rbp + 64]
    mov [rax + 0x40], rcx       ; rbp
    mov rcx, [rbp + 72]
    mov [rax + 0x48], rcx       ; rdi
    mov rcx, [rbp + 80]
    mov [rax + 0x50], rcx       ; rsi
    mov rcx, [rbp + 88]
    mov [rax + 0x58], rcx       ; rdx
    mov rcx, [rbp + 96]
    mov [rax + 0x60], rcx       ; rcx
    mov rcx, [rbp + 104]
    mov [rax + 0x68], rcx       ; rbx
    mov rcx, [rbp + 112]
    mov [rax + 0x70], rcx       ; rax
    mov rcx, [rbp + 120]
    mov [rax + 0x78], rcx       ; RIP
    mov rcx, [rbp + 128]
    mov [rax + 0x80], rcx       ; CS
    mov rcx, [rbp + 136]
    mov [rax + 0x88], rcx       ; RFLAGS
    mov rcx, [rbp + 144]
    mov [rax + 0x90], rcx       ; RSP
    mov rcx, [rbp + 152]
    mov [rax + 0x98], rcx       ; SS

    ; ── Restaure new_ctx ──────────────────────────────────────────────────
    mov rcx, [rdx + 0x00]
    mov [rbp + 0],   rcx        ; r15
    mov rcx, [rdx + 0x08]
    mov [rbp + 8],   rcx        ; r14
    mov rcx, [rdx + 0x10]
    mov [rbp + 16],  rcx        ; r13
    mov rcx, [rdx + 0x18]
    mov [rbp + 24],  rcx        ; r12
    mov rcx, [rdx + 0x20]
    mov [rbp + 32],  rcx        ; r11
    mov rcx, [rdx + 0x28]
    mov [rbp + 40],  rcx        ; r10
    mov rcx, [rdx + 0x30]
    mov [rbp + 48],  rcx        ; r9
    mov rcx, [rdx + 0x38]
    mov [rbp + 56],  rcx        ; r8
    mov rcx, [rdx + 0x40]
    mov [rbp + 64],  rcx        ; rbp
    mov rcx, [rdx + 0x48]
    mov [rbp + 72],  rcx        ; rdi
    mov rcx, [rdx + 0x50]
    mov [rbp + 80],  rcx        ; rsi
    mov rcx, [rdx + 0x58]
    mov [rbp + 88],  rcx        ; rdx
    mov rcx, [rdx + 0x60]
    mov [rbp + 96],  rcx        ; rcx
    mov rcx, [rdx + 0x68]
    mov [rbp + 104], rcx        ; rbx
    mov rcx, [rdx + 0x70]
    mov [rbp + 112], rcx        ; rax
    mov rcx, [rdx + 0x78]
    mov [rbp + 120], rcx        ; RIP
    mov rcx, [rdx + 0x80]
    mov [rbp + 128], rcx        ; CS
    mov rcx, [rdx + 0x88]
    mov [rbp + 136], rcx        ; RFLAGS
    mov rcx, [rdx + 0x90]
    mov [rbp + 144], rcx        ; RSP
    mov rcx, [rdx + 0x98]
    mov [rbp + 152], rcx        ; SS

.eoi:
    mov al, 0x20
    out 0x20, al

    ; --- RESTAURATION VIA RSP ---
    ; Puisque RSP n'a pas bougé depuis les 15 pushs, les registres
    ; sont stockés directement aux offsets positifs de RSP (0 à 112).
    mov r15, [rsp + 0]
    mov r14, [rsp + 8]
    mov r13, [rsp + 16]
    mov r12, [rsp + 24]
    mov r11, [rsp + 32]
    mov r10, [rsp + 40]
    mov r9,  [rsp + 48]
    mov r8,  [rsp + 56]
    mov rbp, [rsp + 64]   ; Restaure le VRAI rbp de l'application (Ring 3)
    mov rdi, [rsp + 72]
    mov rsi, [rsp + 80]
    mov rdx, [rsp + 88]
    mov rcx, [rsp + 96]
    mov rbx, [rsp + 104]

    ; 1. On récupère le VRAI rax (offset 112) et on le met temporairement dans RAX
    mov rax, [rsp + 112]

    ; 2. On avance RSP de 120 octets pour sauter les 15 registres
    ; et pointer pile sur la CPU frame (RIP, CS, RFLAGS...)
    add rsp, 120

    iretq

; ── IRQ 1-15 ─────────────────────────────────────────────────────────────
IRQ 1,  33
IRQ 2,  34
IRQ 3,  35
IRQ 4,  36
IRQ 5,  37
IRQ 6,  38
IRQ 7,  39
IRQ 8,  40
IRQ 9,  41
IRQ 10, 42
IRQ 11, 43
IRQ 12, 44
IRQ 13, 45
IRQ 14, 46
IRQ 15, 47

; ── Handler commun ISR ────────────────────────────────────────────────────
isr_common:
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    mov rdi, rsp
    call interrupt_handler

    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    add rsp, 16
    iretq

; ── Handler commun IRQ 1-15 ──────────────────────────────────────────────
irq_common:
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    mov rdi, rsp
    call interrupt_handler

    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    add rsp, 16
    iretq