#!/usr/bin/env python3
"""
Génère libmath.axl : une .axl de test avec UNE fonction exportée,
`add(a, b) -> a + b`, en code machine x86-64 brut (convention SysV :
a=RDI, b=RSI, retour=RAX).
"""
import struct
import sys

AXF_MAGIC = 0x21465841
AXF_VERSION = 1
AXF_TYPE_AXL = 2
R, W, X = 1, 2, 4

BASE_ADDR = 0x400000  # adresse "logique" -- voir axf_loader.c load_axl_and_resolve
                       # pour le calcul d'offset par rapport a cette base

def build_add_function() -> bytes:
    c = bytearray()
    c += bytes([0x48, 0x89, 0xF8])       # mov rax, rdi
    c += bytes([0x48, 0x01, 0xF0])       # add rax, rsi
    c += bytes([0xC3])                    # ret
    return bytes(c)

def main():
    out_path = sys.argv[1] if len(sys.argv) > 1 else "libmath.axl"

    code = build_add_function()

    header_size = 64
    section_size = 56
    export_size = 40

    section_table_offset = header_size
    export_table_offset = section_table_offset + section_size
    text_file_offset = export_table_offset + export_size

    header = b""
    header += struct.pack("<I", AXF_MAGIC)
    header += struct.pack("<H", AXF_VERSION)
    header += struct.pack("<H", AXF_TYPE_AXL)
    header += struct.pack("<Q", 0)            # entry_point (ignore pour .axl)
    header += struct.pack("<Q", BASE_ADDR)    # base_addr
    header += struct.pack("<I", 1)            # section_count
    header += struct.pack("<I", section_table_offset)
    header += struct.pack("<I", 1)            # export_count
    header += struct.pack("<I", export_table_offset)
    header += struct.pack("<I", 0)            # import_count
    header += struct.pack("<I", 0)            # import_table_offset
    header += struct.pack("<QQ", 0, 0)        # reserved[2]

    assert len(header) == header_size, f"header size mismatch: {len(header)}"

    name = b".text" + b"\x00" * (16 - len(b".text"))
    section = struct.pack(
        "<16sQQQQII",
        name,
        BASE_ADDR,
        len(code),
        text_file_offset,
        len(code),
        R | W | X,
        0,
    )
    assert len(section) == section_size, f"section size mismatch: {len(section)}"

    export_name = b"add" + b"\x00" * (32 - len(b"add"))
    export_entry = struct.pack("<32sQ", export_name, 0)  # offset=0 -- add() est au tout debut de .text
    assert len(export_entry) == export_size, f"export size mismatch: {len(export_entry)}"

    with open(out_path, "wb") as f:
        f.write(header)
        f.write(section)
        f.write(export_entry)
        f.write(code)

    print(f"Écrit {out_path} ({header_size + section_size + export_size + len(code)} octets)")
    print(f"  export: add @ offset 0 (dans .text @ {hex(BASE_ADDR)})")

if __name__ == "__main__":
    main()