; apex-kernel/arch/x86_64/syscall_entry.asm
; ApexOS -- Syscall entry point

[BITS 64]
global syscall_entry
extern syscall_handler
extern in_syscall
extern saved_rcx
extern saved_r11

section .text

syscall_entry:
    cli
    mov r10, rsp
    mov rsp, kernel_syscall_stack_top

    ; Marque qu'on est dans un syscall
    mov qword [rel in_syscall], 1

    ; Sauvegarde RCX et R11 dans des globales
    mov [rel saved_rcx], rcx
    mov [rel saved_r11], r11

    push rbp
    push rbx
    push r12
    push r13
    push r14
    push r15
    push r10        ; RSP userspace

    ; Le CPU (instruction `syscall`) nous donne : RAX=num, RDI=arg1,
    ; RSI=arg2, RDX=arg3, RCX=adresse de retour, R11=RFLAGS user.
    ;
    ; syscall_handler(uint64_t num, uint64_t arg1, uint64_t arg2, uint64_t arg3)
    ; suit la convention d'appel SysV AMD64, qui attend :
    ;   1er argument (num)  -> RDI
    ;   2e argument (arg1)  -> RSI
    ;   3e argument (arg2)  -> RDX
    ;   4e argument (arg3)  -> RCX
    ;
    ; AVANT ce fix, aucun remapping n'etait fait : syscall_handler recevait
    ; donc RDI/RSI/RDX bruts (= num/arg1/arg2 du process, decales d'un cran)
    ; comme si c'etaient ses propres num/arg1/arg2, et RCX (qui contient
    ; l'adresse de retour ecrasee par `syscall`, pas arg3) comme 4e argument.
    ; Resultat : chaque syscall recevait des arguments completement faux
    ; (ex: SYS_WRITE recevait le pointeur du message a la place de sa
    ; longueur, et la vraie longueur a la place du pointeur), donc le
    ; "if (arg1 == 1 || arg1 == 2)" de SYS_WRITE echouait silencieusement
    ; et rien ne s'affichait, meme si le syscall "reussissait" en apparence.
    ;
    ; RCX/R11 sont deja sauvegardes dans saved_rcx/saved_r11 plus haut,
    ; donc on peut librement les ecraser ici pour construire les vrais
    ; arguments C avant le call.
    mov r8,  rdi        ; r8  = arg1 (copie avant d'ecraser rdi)
    mov r9,  rsi        ; r9  = arg2 (copie avant d'ecraser rsi)
    mov rcx, rdx        ; rcx (4e arg C) = arg3
    mov rdx, r9         ; rdx (3e arg C) = arg2
    mov rsi, r8         ; rsi (2e arg C) = arg1
    mov rdi, rax        ; rdi (1er arg C) = num

    ; Appel avec la vraie convention SysV : RDI=num, RSI=arg1, RDX=arg2, RCX=arg3
    call syscall_handler

    pop r10         ; RSP userspace
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp

    ; Restaure RCX et R11 depuis les globales
    mov rcx, [rel saved_rcx]
    mov r11, [rel saved_r11]

    ; Fin syscall
    mov qword [rel in_syscall], 0

    mov rsp, r10
    ; PAS de sti explicite ici -- o64 sysret restaure RFLAGS (donc IF)
    ; depuis R11 automatiquement au moment precis du retour en Ring 3.
    ; Un sti explicite AVANT sysret ouvrait une fenetre de 1-2
    ; instructions ou les interruptions etaient deja actives alors
    ; qu'on etait encore en Ring 0 avec RSP=pile utilisateur mais pas
    ; encore reellement reparti en Ring 3 -- si IRQ0 se declenchait
    ; pile dans cette fenetre, scheduler_tick_asm() voyait deja
    ; in_syscall=0 (mis a 0 juste avant, ligne ci-dessus) et pouvait
    ; tenter un vrai changement de contexte sur un etat CPU
    ; incoherent (freeze aleatoire observe sur `ring3`/`run *.axp`).
    o64 sysret

section .bss
align 16
kernel_syscall_stack:
    resb 8192
kernel_syscall_stack_top:

saved_rcx: resq 1
saved_r11: resq 1