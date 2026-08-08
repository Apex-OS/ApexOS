#!/usr/bin/env python3
"""
Génère stub.axd : driver de test minimal, deux exports :
  - driver_init() : ne fait rien d'observable depuis l'extérieur --
    juste `ret` immédiat. Suffisant pour ce test : le but est de valider
    le PIPELINE (chargement kernel-only, résolution d'exports, appel
    Ring 0 direct), pas un vrai comportement matériel. Pas d'écriture à
    une adresse fixe non mappée (éviterait un page fault inutile) ni
    d'appel à klog_ok() (le code driver est du binaire brut sans lien
    vers les symboles kernel -- pas de vraie table de symboles/
    relocations en v1).
  - driver_ping(arg) -> 42 : fonction appelée plus tard via
    SYS_DRIVER_CALL, pour valider le pipeline complet Ring3 -> syscall
    -> Ring0 -> driver -> retour de valeur jusqu'au caller.
"""
import struct
import sys

AXF_MAGIC = 0x21465841
AXF_VERSION = 1
AXF_TYPE_AXD = 3
R, W, X = 1, 2, 4

BASE_ADDR = 0x400000  # adresse "logique" -- voir axd.c pour le calcul d'offset


def build_code() -> tuple[bytes, int, int]:
    """Retourne (code, offset driver_init, offset driver_ping)."""
    c = bytearray()

    # ── driver_init() -- offset 0 : ret immediat ─────────────────────────
    init_offset = len(c)
    c += bytes([0xC3])  # ret

    # ── driver_ping(arg) -> 42 -- ignore arg (RDI), retourne 42 ─────────
    ping_offset = len(c)
    # mov rax, 42
    c += bytes([0x48, 0xC7, 0xC0]) + struct.pack("<i", 42)
    # ret
    c += bytes([0xC3])

    return bytes(c), init_offset, ping_offset


def main():
    out_path = sys.argv[1] if len(sys.argv) > 1 else "stub.axd"

    code, init_offset, ping_offset = build_code()

    header_size = 64
    section_size = 56
    export_size = 40

    section_table_offset = header_size
    export_table_offset = section_table_offset + section_size
    text_file_offset = export_table_offset + export_size * 2

    header = b""
    header += struct.pack("<I", AXF_MAGIC)
    header += struct.pack("<H", AXF_VERSION)
    header += struct.pack("<H", AXF_TYPE_AXD)
    header += struct.pack("<Q", 0)            # entry_point (ignore pour .axd)
    header += struct.pack("<Q", BASE_ADDR)    # base_addr
    header += struct.pack("<I", 1)            # section_count
    header += struct.pack("<I", section_table_offset)
    header += struct.pack("<I", 2)            # export_count (driver_init + driver_ping)
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

    def export_entry(name: str, offset: int) -> bytes:
        name_b = name.encode() + b"\x00" * (32 - len(name))
        e = struct.pack("<32sQ", name_b, offset)
        assert len(e) == export_size
        return e

    exports = export_entry("driver_init", init_offset)
    exports += export_entry("driver_ping", ping_offset)

    with open(out_path, "wb") as f:
        f.write(header)
        f.write(section)
        f.write(exports)
        f.write(code)

    total = header_size + section_size + export_size * 2 + len(code)
    print(f"Écrit {out_path} ({total} octets)")
    print(f"  export 0: driver_init @ offset {init_offset} (ret immédiat)")
    print(f"  export 1: driver_ping @ offset {ping_offset} (retourne 42)")


if __name__ == "__main__":
    main()