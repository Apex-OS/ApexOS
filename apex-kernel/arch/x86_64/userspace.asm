; apex-kernel/arch/x86_64/userspace.asm
; ApexOS -- Jump vers Ring 3

[BITS 64]
global userspace_jump

; userspace_jump(uint64_t entry, uint64_t stack)
; RDI = entry, RSI = stack
userspace_jump:
    ; Segments user avec RPL=3
    mov ax, 0x23        ; User data segment (0x20 | 3)
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; Construit le cadre iretq :
    ; SS, RSP, RFLAGS, CS, RIP
    push 0x23           ; SS  = user data | RPL3
    push rsi            ; RSP = stack userspace
    push 0x202          ; RFLAGS = IF=1
    push 0x1B           ; CS  = user code | RPL3 (0x18 | 3)
    push rdi            ; RIP = entry userspace

    iretq