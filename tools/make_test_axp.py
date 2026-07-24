#!/usr/bin/env python3
"""
Génère un fichier .axp minimal de test pour valider le loader AXF.

Le programme généré fait, en code machine x86-64 brut :
    SYS_WRITE (num=1) avec un message "Hello from AXF!\n"
    SYS_EXIT (num=0)

Layout du fichier :
    [0..64)    axf_header_t
    [64..120)  axf_section_t (une seule section : .text, contient tout,
               code + message, RWX pour rester simple en V1)
    [120..X)   contenu de .text (code + data inline)
"""
import struct
import sys

AXF_MAGIC = 0x21465841
AXF_VERSION = 1
AXF_TYPE_AXP = 1
AXF_SECTION_READ = 1
AXF_SECTION_WRITE = 2
AXF_SECTION_EXEC = 4

BASE_ADDR = 0x400000

def syscall_write_exit_code(msg: bytes) -> bytes:
    """
    Code machine x86-64 équivalent à user_main actuel :
        do_syscall(1, 1, addr_msg, len(msg))
        do_syscall(0, 0, 0, 0)
        for(;;);

    On place le message JUSTE APRES le code, et on calcule son adresse
    absolue (BASE_ADDR + offset) codée en dur -- pas de vraies
    relocations en V1, le loader charge toujours à BASE_ADDR fixe.
    """
    code = bytearray()

    # Le message sera juste après le code. On ne connaît sa taille qu'après
    # avoir fini d'écrire le code -- on fait donc un premier passage pour
    # calculer la taille du code, PUIS on génère le code avec la bonne
    # adresse immédiate pour le message.

    def build(msg_addr: int) -> bytes:
        c = bytearray()
        # mov rax, 1      (SYS_WRITE)
        c += bytes([0x48, 0xC7, 0xC0]) + struct.pack("<i", 1)
        # mov rdi, 1      (fd = stdout)
        c += bytes([0x48, 0xC7, 0xC7]) + struct.pack("<i", 1)
        # mov rsi, msg_addr   (movabs, 10 octets)
        c += bytes([0x48, 0xBE]) + struct.pack("<Q", msg_addr)
        # mov rdx, len(msg)
        c += bytes([0x48, 0xC7, 0xC2]) + struct.pack("<i", len(msg))
        # syscall
        c += bytes([0x0F, 0x05])
        # mov rax, 0      (SYS_EXIT)
        c += bytes([0x48, 0xC7, 0xC0]) + struct.pack("<i", 0)
        # mov rdi, 0      (code retour)
        c += bytes([0x48, 0xC7, 0xC7]) + struct.pack("<i", 0)
        # syscall
        c += bytes([0x0F, 0x05])
        # jmp $ (boucle infinie de securite, ne devrait jamais etre atteinte)
        c += bytes([0xEB, 0xFE])
        return bytes(c)

    # Premier passage pour connaître la taille du code (avec une adresse
    # bidon), puis calcul de la vraie adresse du message.
    dummy = build(0)
    msg_addr = BASE_ADDR + len(dummy)
    code = build(msg_addr)

    assert len(code) == len(dummy), "la taille du code ne doit pas dependre de l'adresse immediate (movabs = toujours 10 octets)"

    return code + msg

def main():
    out_path = sys.argv[1] if len(sys.argv) > 1 else "test.axp"

    msg = b"Hello from AXF!\n"
    text_data = syscall_write_exit_code(msg)

    # ── Header ──────────────────────────────────────────────────────────
    header_size = 64
    section_size = 56
    section_table_offset = header_size
    text_file_offset = section_table_offset + section_size

    entry_point = BASE_ADDR  # le code commence tout au début de .text

    header = struct.pack(
        "<IHHQQII4Q",
        AXF_MAGIC,
        AXF_VERSION,
        AXF_TYPE_AXP,
        entry_point,
        BASE_ADDR,
        1,  # section_count
        section_table_offset,
        0, 0, 0, 0,  # reserved[4]
    )
    assert len(header) == header_size, f"header size mismatch: {len(header)}"

    # ── Section .text (RWX en V1 pour rester simple -- code + data inline) ──
    name = b".text" + b"\x00" * (16 - len(b".text"))
    section = struct.pack(
        "<16sQQQQII",
        name,
        BASE_ADDR,            # vaddr
        len(text_data),       # mem_size
        text_file_offset,     # file_offset
        len(text_data),       # file_size
        AXF_SECTION_READ | AXF_SECTION_WRITE | AXF_SECTION_EXEC,
        0,  # _pad0
    )
    assert len(section) == section_size, f"section size mismatch: {len(section)}"

    with open(out_path, "wb") as f:
        f.write(header)
        f.write(section)
        f.write(text_data)

    print(f"Écrit {out_path} ({header_size + section_size + len(text_data)} octets)")
    print(f"  entry_point = {hex(entry_point)}")
    print(f"  .text @ {hex(BASE_ADDR)}, {len(text_data)} octets (code + message)")

if __name__ == "__main__":
    main()