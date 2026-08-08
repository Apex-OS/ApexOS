#!/usr/bin/env python3
"""
Génère test_import.axp : un .axp qui IMPORTE add() depuis libmath.axl,
l'appelle avec (3, 4), affiche le résultat en ASCII, puis exit.

Layout mémoire au démarrage :
    AXF_IMPORT_TABLE_ADDR (0x401000) : uint64_t[0] = adresse réelle de add()

Code (pseudo, en x86-64 réel ci-dessous) :
    mov rax, [0x401000]      ; adresse résolue de add()
    mov rdi, 3
    mov rsi, 4
    call rax                 ; appelle add(3,4) -> résultat dans RAX
    add al, '0'              ; convertit le chiffre (0-9) en caractère ASCII
    mov [msg_digit], al      ; écrit le caractère dans le message
    ; SYS_WRITE
    mov rax, 1
    mov rdi, 1
    mov rsi, msg_addr
    mov rdx, len(msg)
    syscall
    ; SYS_EXIT
    mov rax, 0
    mov rdi, 0
    syscall
    jmp $
"""
import struct
import sys

AXF_MAGIC = 0x21465841
AXF_VERSION = 1
AXF_TYPE_AXP = 1
R, W, X = 1, 2, 4

BASE_ADDR = 0x400000
IMPORT_TABLE_ADDR = 0x401000

def build_code(msg_addr: int, msg_digit_offset_in_msg: int, msg_len: int) -> bytes:
    c = bytearray()
    # mov rax, [0x401000]   -- lit l'adresse resolue de add()
    c += bytes([0x48, 0xA1]) + struct.pack("<Q", IMPORT_TABLE_ADDR)
    # mov rdi, 3
    c += bytes([0x48, 0xC7, 0xC7]) + struct.pack("<i", 3)
    # mov rsi, 4
    c += bytes([0x48, 0xC7, 0xC6]) + struct.pack("<i", 4)
    # call rax
    c += bytes([0xFF, 0xD0])
    # add al, '0'  (convertit 0-9 en ASCII -- resultat attendu = 7 -> '7')
    c += bytes([0x04, ord('0')])
    # mov [msg_addr + msg_digit_offset_in_msg], al
    digit_addr = msg_addr + msg_digit_offset_in_msg
    c += bytes([0xA2]) + struct.pack("<Q", digit_addr)
    # mov rax, 1  (SYS_WRITE)
    c += bytes([0x48, 0xC7, 0xC0]) + struct.pack("<i", 1)
    # mov rdi, 1  (fd stdout)
    c += bytes([0x48, 0xC7, 0xC7]) + struct.pack("<i", 1)
    # mov rsi, msg_addr
    c += bytes([0x48, 0xBE]) + struct.pack("<Q", msg_addr)
    # mov rdx, len(msg)  -- deplace ici (etait dans `rest` avant -- BUG :
    # msg_addr etait calcule avec seulement la taille de cette fonction
    # SANS le `rest` (mov rdx + 2x syscall + jmp $), donc msg_addr
    # pointait 27 octets trop tot, EN PLEIN MILIEU DU CODE au lieu
    # d'apres le code entier. Desormais build_code() genere la totalite
    # du code (y compris rest), donc son len() est la vraie taille utilisee
    # pour calculer msg_addr, sans decalage possible.
    c += bytes([0x48, 0xC7, 0xC2]) + struct.pack("<i", msg_len)
    # syscall (SYS_WRITE)
    c += bytes([0x0F, 0x05])
    # mov rax, 0 (SYS_EXIT)
    c += bytes([0x48, 0xC7, 0xC0]) + struct.pack("<i", 0)
    # mov rdi, 0
    c += bytes([0x48, 0xC7, 0xC7]) + struct.pack("<i", 0)
    # syscall (SYS_EXIT)
    c += bytes([0x0F, 0x05])
    # jmp $
    c += bytes([0xEB, 0xFE])
    return bytes(c)

def main():
    out_path = sys.argv[1] if len(sys.argv) > 1 else "test_import.axp"

    msg = bytearray(b"add(3,4) = X\n")
    digit_offset = msg.index(ord('X'))

    # Deux passages : d'abord code "factice" pour connaitre sa taille
    # TOTALE (dummy inclut maintenant TOUT le code, rest compris), puis
    # vrai code avec la bonne adresse immediate pour le message (movabs
    # = toujours 10 octets, donc la taille du code ne change pas entre
    # les deux passages -- meme technique que make_test_axp.py).
    dummy = build_code(0, digit_offset, len(msg))
    msg_addr = BASE_ADDR + len(dummy)
    code = build_code(msg_addr, digit_offset, len(msg))
    assert len(code) == len(dummy)

    text_data = code + bytes(msg)

    entry_point = BASE_ADDR

    header_size = 64
    section_size = 56
    import_size = 64

    section_table_offset = header_size
    import_table_offset = section_table_offset + section_size
    text_file_offset = import_table_offset + import_size

    header = b""
    header += struct.pack("<I", AXF_MAGIC)
    header += struct.pack("<H", AXF_VERSION)
    header += struct.pack("<H", AXF_TYPE_AXP)
    header += struct.pack("<Q", entry_point)
    header += struct.pack("<Q", BASE_ADDR)
    header += struct.pack("<I", 1)                # section_count
    header += struct.pack("<I", section_table_offset)
    header += struct.pack("<I", 0)                # export_count (aucun -- .axp)
    header += struct.pack("<I", 0)                # export_table_offset
    header += struct.pack("<I", 1)                # import_count
    header += struct.pack("<I", import_table_offset)
    header += struct.pack("<QQ", 0, 0)            # reserved[2]
    assert len(header) == header_size

    name = b".text" + b"\x00" * (16 - len(b".text"))
    section = struct.pack(
        "<16sQQQQII",
        name, BASE_ADDR, len(text_data), text_file_offset, len(text_data),
        R | W | X, 0,
    )
    assert len(section) == section_size

    lib_name = b"libmath.axl" + b"\x00" * (32 - len(b"libmath.axl"))
    func_name = b"add" + b"\x00" * (32 - len(b"add"))
    import_entry = lib_name + func_name
    assert len(import_entry) == import_size, f"{len(import_entry)}"

    with open(out_path, "wb") as f:
        f.write(header)
        f.write(section)
        f.write(import_entry)
        f.write(text_data)

    print(f"Écrit {out_path} ({header_size + section_size + import_size + len(text_data)} octets)")
    print(f"  imports: add from libmath.axl")
    print(f"  entry_point = {hex(entry_point)}")
    print(f"  msg_addr = {hex(msg_addr)}  (code size = {len(code)} octets)")

if __name__ == "__main__":
    main()