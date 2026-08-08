; apex-kernel/arch/x86_64/boot/stage1.asm
; ApexOS Stage 1 Bootloader — MBR (512 bytes)
; Chargé par le BIOS à 0x7C00 en mode réel 16-bit

[BITS 16]
[ORG 0x7C00]

; ── Constantes ───────────────────────────────────────────────────────────────
STAGE2_LOAD_SEG  equ 0x0000
STAGE2_LOAD_OFF  equ 0x7E00     ; Juste après stage1
STAGE2_SECTORS   equ 32         ; Nombre de secteurs à lire pour stage2 -- 16KB,
                                 ; room to grow (was 16/8KB, silently overflowed
                                 ; once stage2.asm grew past that with VBE mode
                                 ; detection code -- see stage2.asm history)

; ── Entry point ──────────────────────────────────────────────────────────────
start:
    cli                          ; Désactive les interruptions

    ; Normalise les segments
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00               ; Stack juste avant notre code

    sti                          ; Réactive les interruptions

    ; Sauvegarde le numéro de drive (fourni par le BIOS dans DL)
    mov [boot_drive], dl

    ; Affiche un message de démarrage
    mov si, msg_boot
    call print_string

    ; Charge stage2 depuis le disque
    call load_stage2

    ; Saute vers stage2
    jmp STAGE2_LOAD_SEG:STAGE2_LOAD_OFF

    ; Ne devrait jamais arriver ici
    cli
    hlt

; ── Chargement de stage2 via BIOS INT 13h ────────────────────────────────────
load_stage2:
    mov ax, STAGE2_LOAD_SEG
    mov es, ax
    mov bx, STAGE2_LOAD_OFF      ; ES:BX = destination

    mov ah, 0x02                 ; Fonction : lire secteurs
    mov al, STAGE2_SECTORS       ; Nombre de secteurs
    mov ch, 0                    ; Cylindre 0
    mov cl, 2                    ; Secteur 2 (stage2 commence après le MBR)
    mov dh, 0                    ; Tête 0
    mov dl, [boot_drive]         ; Drive
    int 0x13

    jc disk_error                ; Carry flag = erreur
    ret

; ── Erreur disque ─────────────────────────────────────────────────────────────
disk_error:
    mov si, msg_disk_error
    call print_string
    cli
    hlt

; ── Print string (SI = pointeur vers string null-terminated) ─────────────────
print_string:
    mov ah, 0x0E                 ; BIOS teletype mode
.loop:
    lodsb                        ; Charge AL depuis [SI], incrémente SI
    test al, al                  ; Fin de string ?
    jz .done
    int 0x10                     ; Affiche le caractère
    jmp .loop
.done:
    ret

; ── Données ──────────────────────────────────────────────────────────────────
boot_drive      db 0
msg_boot        db "ApexOS Stage1 OK", 0x0D, 0x0A, 0
msg_disk_error  db "Disk error!", 0x0D, 0x0A, 0

; ── Padding + signature MBR ──────────────────────────────────────────────────
times 510 - ($ - $$) db 0       ; Rempli de zéros jusqu'à l'octet 510
dw 0xAA55                       ; Signature MBR obligatoire