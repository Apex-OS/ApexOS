; apex-kernel/arch/x86_64/userspace.asm
; ApexOS -- Jump vers Ring 3

[BITS 64]
global userspace_jump
global user_main_start
global user_main_end

; userspace_jump(uint64_t entry, uint64_t stack)
; RDI = entry, RSI = stack
userspace_jump:
    ; Segments user avec RPL=3.
    ;
    ; gs delibermement PAS recharge ici, contrairement a ds/es/fs --
    ; IA32_GS_BASE est configure par-CPU via wrmsr() par
    ; Core/SMP/PerCPU/PerCpu.c's percpu_init_this_cpu(), independamment
    ; du descripteur GDT associe au SELECTEUR gs. Or `mov ax, 0x23` /
    ; `mov gs, ax` recharge la base cachee du segment gs depuis ce
    ; descripteur (plat, base=0) comme effet de bord sur le vrai
    ; materiel -- ecrasant silencieusement le GS_BASE per-CPU dont
    ; percpu_get() a besoin pour le reste de la vie de ce coeur. Rien
    ; ne restaure GS_BASE apres coup : le programme Ring 3 lui-meme ne
    ; touche jamais a gs, et Core/Syscalls/Handlers/SysExit.c's
    ; sys_exit() ne recharge plus gs non plus (meme fix applique la-bas
    ; en premier -- voir son propre commentaire). Sans cette omission
    ; ici, TOUT programme Ring 3 casse percpu_get() sur ce coeur des
    ; l'instant ou userspace_jump() est appele, pas seulement apres
    ; SYS_EXIT -- confirme via gdb comme la cause du #GP deterministe
    ; dans Core/Tasking/Scheduler/SchedulerTick.c's scheduler_tick_asm()
    ; (~546 ticks apres le retour d'un programme, des que le timer est
    ; reactive) : current_proc/proc_list restaient sains, mais
    ; percpu_get() lui-meme deferencait un GS_BASE deja corrompu depuis
    ; ce saut. ds/es/fs n'ont pas ce risque : seule la base de gs est
    ; jamais lue via l'adressage `mov gs:offset` dans ce kernel.
    mov ax, 0x23        ; User data segment (0x20 | 3)
    mov ds, ax
    mov es, ax
    mov fs, ax

    ; Construit le cadre iretq :
    ; SS, RSP, RFLAGS, CS, RIP
    push 0x23           ; SS  = user data | RPL3
    push rsi            ; RSP = stack userspace
    push 0x202          ; RFLAGS = IF=1
    push 0x1B           ; CS  = user code | RPL3 (0x18 | 3)
    push rdi            ; RIP = entry userspace

    iretq

; Minimal position-independent Ring 3 smoke test used by `ring3`.
; RAX = syscall number, so zero selects SYS_EXIT.
user_main_start:
    mov eax, 0          ; RAX = SYS_EXIT
    xor edi, edi        ; RDI = exit code = 0 (was left holding
                         ; USER_CODE_ADDR from userspace_jump's RDI=entry
                         ; argument, never overwritten before this syscall --
                         ; that's what showed up as "exit: 4194304" = 0x400000)
    syscall
    hlt
user_main_end: