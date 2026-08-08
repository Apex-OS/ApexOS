#!/usr/bin/env python3
"""
Génère test_driver.axp : un .axp qui appelle driver_ping() du driver
stub.axd (déjà chargé au boot, voir kernel.c) via SYS_DRIVER_CALL, donc
en traversant le vrai chemin Ring3 -> syscall -> Ring0 -> axd_call() --
contrairement au test précédent qui appelait axd_call() directement
depuis le kernel. Affiche le résultat en ASCII, puis exit.

Layout : aucune adresse mémoire externe à lire (contrairement à
test_import.axp avec sa table d'imports) -- driver_id et func_id sont
codés en dur dans les registres, connus au moment de la génération
(stub.axd est toujours le driver_id 0, driver_ping est l'export 1, voir
kernel.c et make_test_axd.py).

Code (pseudo) :
    ; SYS_DRIVER_CALL(driver_id=0, func_id=1, arg=0)
    mov rax, 5      ; SYS_DRIVER_CALL
    mov rdi, 0      ; driver_id
    mov rsi, 1      ; func_id (driver_ping)
    mov rdx, 0      ; arg (ignoré par driver_ping)
    syscall         ; -> RAX = 42
    ; convertit RAX (0-99) en 2 chiffres ASCII et les écrit dans msg
    ; SYS_WRITE
    ...
    ; SYS_EXIT
    ...

IMPORTANT (piège déjà rencontré avec test_import.axp) : build_code() est
appelée en DEUX passages avec la MEME taille de code dans les deux cas
(le seul octet qui change est l'adresse immédiate de msg_addr, encodée
sur une largeur FIXE -- movabs 10 octets -- donc len(dummy) ==
len(code) toujours). Ne JAMAIS calculer msg_addr à partir d'une version
partielle du code.
"""
import struct
import sys

AXF_MAGIC = 0x21465841
AXF_VERSION = 1
AXF_TYPE_AXP = 1
R, W, X = 1, 2, 4

BASE_ADDR = 0x400000
SYS_DRIVER_CALL = 5
SYS_WRITE = 1
SYS_EXIT = 0

DRIVER_ID = 0   # stub.axd est le premier (et seul) driver chargé au boot
FUNC_ID = 1     # export 1 = driver_ping (export 0 = driver_init, voir make_test_axd.py)


def build_code(msg_addr: int, digit_tens_offset: int, digit_units_offset: int, msg_len: int) -> bytes:
    c = bytearray()

    # mov rax, SYS_DRIVER_CALL
    c += bytes([0x48, 0xC7, 0xC0]) + struct.pack("<i", SYS_DRIVER_CALL)
    # mov rdi, DRIVER_ID
    c += bytes([0x48, 0xC7, 0xC7]) + struct.pack("<i", DRIVER_ID)
    # mov rsi, FUNC_ID
    c += bytes([0x48, 0xC7, 0xC6]) + struct.pack("<i", FUNC_ID)
    # mov rdx, 0  (arg, ignoré par driver_ping)
    c += bytes([0x48, 0xC7, 0xC2]) + struct.pack("<i", 0)
    # syscall -> RAX = résultat de driver_ping() (42 attendu)
    c += bytes([0x0F, 0x05])

    # ── Convertit RAX (0-99) en deux chiffres ASCII : dizaine puis unité ──
    # mov rcx, rax       ; copie pour préserver rax pendant les divisions
    c += bytes([0x48, 0x89, 0xC1])
    # mov rbx, 10
    c += bytes([0x48, 0xC7, 0xC3]) + struct.pack("<i", 10)
    # xor rdx, rdx
    c += bytes([0x48, 0x31, 0xD2])
    # mov rax, rcx
    c += bytes([0x48, 0x89, 0xC8])
    # div rbx            ; rax = quotient (dizaine), rdx = reste (unité)
    c += bytes([0x48, 0xF7, 0xF3])
    # add al, '0'
    c += bytes([0x04, ord('0')])
    # mov [msg_addr + digit_tens_offset], al
    c += bytes([0xA2]) + struct.pack("<Q", msg_addr + digit_tens_offset)
    # mov al, dl         ; reste (unité) -> al
    c += bytes([0x88, 0xD0])
    # add al, '0'
    c += bytes([0x04, ord('0')])
    # mov [msg_addr + digit_units_offset], al
    c += bytes([0xA2]) + struct.pack("<Q", msg_addr + digit_units_offset)

    # ── SYS_WRITE ─────────────────────────────────────────────────────────
    # mov rax, SYS_WRITE
    c += bytes([0x48, 0xC7, 0xC0]) + struct.pack("<i", SYS_WRITE)
    # mov rdi, 1  (stdout)
    c += bytes([0x48, 0xC7, 0xC7]) + struct.pack("<i", 1)
    # mov rsi, msg_addr
    c += bytes([0x48, 0xBE]) + struct.pack("<Q", msg_addr)
    # mov rdx, msg_len
    c += bytes([0x48, 0xC7, 0xC2]) + struct.pack("<i", msg_len)
    # syscall (SYS_WRITE)
    c += bytes([0x0F, 0x05])

    # ── SYS_EXIT ──────────────────────────────────────────────────────────
    # mov rax, SYS_EXIT
    c += bytes([0x48, 0xC7, 0xC0]) + struct.pack("<i", SYS_EXIT)
    # mov rdi, 0
    c += bytes([0x48, 0xC7, 0xC7]) + struct.pack("<i", 0)
    # syscall (SYS_EXIT)
    c += bytes([0x0F, 0x05])
    # jmp $  (garde-fou si jamais on revient ici)
    c += bytes([0xEB, 0xFE])

    return bytes(c)


def main():
    out_path = sys.argv[1] if len(sys.argv) > 1 else "test_driver.axp"

    msg = bytearray(b"driver_ping() = XX\n")
    tens_offset = msg.index(ord('X'))
    units_offset = msg.index(ord('X'), tens_offset + 1)

    # Deux passages, MEME taille dans les deux cas (voir docstring) :
    # movabs (0xA2.../0xBE...) encode toujours l'adresse sur 8 octets,
    # donc changer msg_addr entre les deux passages ne change PAS la
    # taille du code généré.
    dummy = build_code(0, tens_offset, units_offset, len(msg))
    msg_addr = BASE_ADDR + len(dummy)
    code = build_code(msg_addr, tens_offset, units_offset, len(msg))
    assert len(code) == len(dummy), f"taille code changée entre les 2 passages: {len(dummy)} != {len(code)}"

    text_data = code + bytes(msg)

    entry_point = BASE_ADDR

    header_size = 64
    section_size = 56

    section_table_offset = header_size
    text_file_offset = section_table_offset + section_size

    header = b""
    header += struct.pack("<I", AXF_MAGIC)
    header += struct.pack("<H", AXF_VERSION)
    header += struct.pack("<H", AXF_TYPE_AXP)
    header += struct.pack("<Q", entry_point)
    header += struct.pack("<Q", BASE_ADDR)
    header += struct.pack("<I", 1)      # section_count
    header += struct.pack("<I", section_table_offset)
    header += struct.pack("<I", 0)      # export_count (aucun -- .axp)
    header += struct.pack("<I", 0)      # export_table_offset
    header += struct.pack("<I", 0)      # import_count (aucun -- pas de .axl ici)
    header += struct.pack("<I", 0)      # import_table_offset
    header += struct.pack("<QQ", 0, 0)  # reserved[2]
    assert len(header) == header_size

    name = b".text" + b"\x00" * (16 - len(b".text"))
    section = struct.pack(
        "<16sQQQQII",
        name, BASE_ADDR, len(text_data), text_file_offset, len(text_data),
        R | W | X, 0,
    )
    assert len(section) == section_size

    with open(out_path, "wb") as f:
        f.write(header)
        f.write(section)
        f.write(text_data)

    total = header_size + section_size + len(text_data)
    print(f"Écrit {out_path} ({total} octets)")
    print(f"  SYS_DRIVER_CALL(driver_id={DRIVER_ID}, func_id={FUNC_ID}, arg=0)")
    print(f"  entry_point = {hex(entry_point)}")
    print(f"  msg_addr = {hex(msg_addr)}  (code size = {len(code)} octets)")


if __name__ == "__main__":
    main()