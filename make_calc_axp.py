#!/usr/bin/env python3
"""
Génère calc.axp : calculatrice interactive.

Demande un premier chiffre (0-9), un opérateur (+, -, *), un second
chiffre (0-9), calcule le résultat et l'affiche, puis exit.

Suit exactement le style/layout des autres générateurs de ce dossier
(make_test_axp.py, make_test_driver_axp.py) : code machine x86-64 brut
fabriqué à la main, technique du double-passage pour connaître la
taille du code avant de coder en dur les adresses immédiates
(movabs = toujours 10 octets, donc la taille ne change jamais entre
les deux passages -- voir make_test_axp_with_import.py's docstring
pour l'historique du bug que cette technique évite).

Nouveau par rapport aux générateurs existants : ce programme utilise
SYS_READ (num=2), jusqu'ici jamais exercé par aucun .axp de ce projet
(SysRead.c, kernel-side, vient d'être implémenté spécifiquement pour
ce programme -- SYS_READ existait comme constante depuis le début mais
n'avait jamais de handler branché avant). SYS_READ bloque (busy-wait
côté kernel, Drivers/Input/Keyboard/KeyBuffer.c's keyboard_getchar())
jusqu'à ce qu'une touche soit disponible -- ce code boucle lui-même
tant que la touche lue n'est pas un chiffre/opérateur valide, en
renvoyant un SYS_READ à chaque tentative (V1 : pas de vraie boucle
d'attente événementielle plus fine que ça côté .axp).
"""
import struct
import sys

AXF_MAGIC = 0x21465841
AXF_VERSION = 1
AXF_TYPE_AXP = 1
R, W, X = 1, 2, 4

BASE_ADDR = 0x400000

SYS_EXIT = 0
SYS_WRITE = 1
SYS_READ = 2


def imm32(value: int) -> bytes:
    return struct.pack("<i", value)


def mov_rax_imm32(value: int) -> bytes:
    return bytes([0x48, 0xC7, 0xC0]) + imm32(value)


def mov_rdi_imm32(value: int) -> bytes:
    return bytes([0x48, 0xC7, 0xC7]) + imm32(value)


def mov_rsi_imm32(value: int) -> bytes:
    return bytes([0x48, 0xC7, 0xC6]) + imm32(value)


def mov_rdx_imm32(value: int) -> bytes:
    return bytes([0x48, 0xC7, 0xC2]) + imm32(value)


def mov_rsi_imm64(value: int) -> bytes:
    # movabs rsi, imm64 -- always 10 bytes, this is what makes the
    # double-pass technique work (see this file's own docstring).
    return bytes([0x48, 0xBE]) + struct.pack("<Q", value)


SYSCALL = bytes([0x0F, 0x05])


def build_code(msg_prompt1_addr, msg_prompt1_len,
                msg_prompt_op_addr, msg_prompt_op_len,
                msg_prompt2_addr, msg_prompt2_len,
                msg_equals_addr, msg_equals_len,
                msg_newline_addr, msg_newline_len,
                key_buf_addr, num_buf_addr) -> bytes:
    c = bytearray()

    def print_msg(addr, length):
        # SYS_WRITE(fd=1, buf=addr, len=length)
        nonlocal c
        c += mov_rax_imm32(SYS_WRITE)
        c += mov_rdi_imm32(1)
        c += mov_rsi_imm64(addr)
        c += mov_rdx_imm32(length)
        c += SYSCALL

    def read_one_key():
        # SYS_READ(fd=0, buf=key_buf_addr, len=1) -- result byte lands
        # at key_buf_addr, this function doesn't need SYS_READ's own
        # return value (the count), only the byte it wrote.
        nonlocal c
        c += mov_rax_imm32(SYS_READ)
        c += mov_rdi_imm32(0)
        c += mov_rsi_imm64(key_buf_addr)
        c += mov_rdx_imm32(1)
        c += SYSCALL

    # ── read_digit: loop reading one key at a time until it's '0'-'9',
    #    echo it back (+ a newline, so the next prompt starts on its
    #    own line instead of running together with what was just
    #    typed), leave its NUMERIC value (0-9, not ASCII) in AL ────────
    def read_digit_inline(dest_addr):
        nonlocal c
        retry_pos = len(c)
        read_one_key()
        # cmp byte [key_buf], '0'
        c += bytes([0x80, 0x3C, 0x25]) + struct.pack("<I", key_buf_addr) + bytes([ord('0')])
        # jl retry (short jump -- patched below once we know the distance)
        jl_pos = len(c)
        c += bytes([0x7C, 0x00])  # placeholder
        # cmp byte [key_buf], '9'
        c += bytes([0x80, 0x3C, 0x25]) + struct.pack("<I", key_buf_addr) + bytes([ord('9')])
        jg_pos = len(c)
        c += bytes([0x7F, 0x00])  # placeholder

        # Valid digit: echo it back, then a newline.
        print_msg(key_buf_addr, 1)
        print_msg(msg_newline_addr, msg_newline_len)

        # mov al, [key_buf]
        c += bytes([0x8A, 0x04, 0x25]) + struct.pack("<I", key_buf_addr)
        # sub al, '0' -- ASCII digit -> numeric value
        c += bytes([0x2C, ord('0')])
        # mov [dest_addr], al
        c += bytes([0xA2]) + struct.pack("<Q", dest_addr)

        end_pos = len(c)

        # Patch the two short jumps now that we know where "retry" is
        # relative to each of them, and where "end" is.
        jl_rel = retry_pos - (jl_pos + 2)
        c[jl_pos + 1] = jl_rel & 0xFF
        jg_rel = retry_pos - (jg_pos + 2)
        c[jg_pos + 1] = jg_rel & 0xFF

    # ── read_op: loop reading one key at a time until it's +, -, or *,
    #    echo it back (+ a newline, same reasoning as read_digit_inline
    #    above), leave the ASCII character itself in AL ────────────────
    def read_op_inline(dest_addr):
        nonlocal c
        retry_pos = len(c)
        read_one_key()
        # cmp byte [key_buf], '+'
        c += bytes([0x80, 0x3C, 0x25]) + struct.pack("<I", key_buf_addr) + bytes([ord('+')])
        je_plus_pos = len(c)
        c += bytes([0x74, 0x00])
        # cmp byte [key_buf], '-'
        c += bytes([0x80, 0x3C, 0x25]) + struct.pack("<I", key_buf_addr) + bytes([ord('-')])
        je_minus_pos = len(c)
        c += bytes([0x74, 0x00])
        # cmp byte [key_buf], '*'
        c += bytes([0x80, 0x3C, 0x25]) + struct.pack("<I", key_buf_addr) + bytes([ord('*')])
        je_star_pos = len(c)
        c += bytes([0x74, 0x00])
        # none matched -- retry
        jmp_retry_pos = len(c)
        c += bytes([0xEB, 0x00])

        accept_pos = len(c)
        print_msg(key_buf_addr, 1)
        print_msg(msg_newline_addr, msg_newline_len)
        # mov al, [key_buf]
        c += bytes([0x8A, 0x04, 0x25]) + struct.pack("<I", key_buf_addr)
        # mov [dest_addr], al
        c += bytes([0xA2]) + struct.pack("<Q", dest_addr)

        end_pos = len(c)

        for pos in (je_plus_pos, je_minus_pos, je_star_pos):
            rel = accept_pos - (pos + 2)
            c[pos + 1] = rel & 0xFF
        jmp_rel = retry_pos - (jmp_retry_pos + 2)
        c[jmp_retry_pos + 1] = jmp_rel & 0xFF

    operand_a_addr = key_buf_addr + 1
    operand_b_addr = key_buf_addr + 2
    operator_addr = key_buf_addr + 3
    result_addr = key_buf_addr + 4

    print_msg(msg_prompt1_addr, msg_prompt1_len)
    read_digit_inline(operand_a_addr)

    print_msg(msg_prompt_op_addr, msg_prompt_op_len)
    read_op_inline(operator_addr)

    print_msg(msg_prompt2_addr, msg_prompt2_len)
    read_digit_inline(operand_b_addr)

    # ── Compute: AL = operand_a, load operand_b into CL, operator into DL ──
    # movzx eax, byte [operand_a_addr]
    c += bytes([0x0F, 0xB6, 0x04, 0x25]) + struct.pack("<I", operand_a_addr)
    # movzx ecx, byte [operand_b_addr]
    c += bytes([0x0F, 0xB6, 0x0C, 0x25]) + struct.pack("<I", operand_b_addr)
    # mov dl, [operator_addr]
    c += bytes([0x8A, 0x14, 0x25]) + struct.pack("<I", operator_addr)

    # cmp dl, '+'
    c += bytes([0x80, 0xFA, ord('+')])
    je_add_pos = len(c)
    c += bytes([0x74, 0x00])
    # cmp dl, '-'
    c += bytes([0x80, 0xFA, ord('-')])
    je_sub_pos = len(c)
    c += bytes([0x74, 0x00])
    # else: '*' -- imul eax, ecx
    c += bytes([0x0F, 0xAF, 0xC1])
    jmp_done_pos = len(c)
    c += bytes([0xEB, 0x00])

    add_pos = len(c)
    # add eax, ecx
    c += bytes([0x01, 0xC8])
    jmp_done2_pos = len(c)
    c += bytes([0xEB, 0x00])

    sub_pos = len(c)
    # sub eax, ecx
    c += bytes([0x29, 0xC8])

    done_pos = len(c)

    c[je_add_pos + 1] = (add_pos - (je_add_pos + 2)) & 0xFF
    c[je_sub_pos + 1] = (sub_pos - (je_sub_pos + 2)) & 0xFF
    c[jmp_done_pos + 1] = (done_pos - (jmp_done_pos + 2)) & 0xFF
    c[jmp_done2_pos + 1] = (done_pos - (jmp_done2_pos + 2)) & 0xFF

    # Saves the computed result (EAX) to memory BEFORE the "= " print
    # below -- print_msg() issues a SYS_WRITE syscall, and every
    # syscall clobbers RAX with its own return value (here, the byte
    # count SYS_WRITE actually wrote) on the way back to Ring 3. The
    # result computed just above would otherwise be silently
    # overwritten by that return value before the itoa loop further
    # down ever got to read it -- which is exactly what made this
    # program print something that happened to look like a small,
    # unrelated number instead of the real sum/difference/product
    # (regressed once already when this file was regenerated without
    # this block -- keep it here, right next to the "= " print it
    # exists for).
    # mov [result_addr], eax
    c += bytes([0x89, 0x04, 0x25]) + struct.pack("<I", result_addr)

    # ── Print " = " ──────────────────────────────────────────────────
    print_msg(msg_equals_addr, msg_equals_len)

    # Restores the real result into EAX now that the syscall above is
    # done clobbering it -- see this block's own comment just above
    # print_msg() for why this is necessary at all.
    # mov eax, [result_addr]
    c += bytes([0x8B, 0x04, 0x25]) + struct.pack("<I", result_addr)

    # ── itoa(EAX) into num_buf (16 bytes), then print it ────────────
    # Writes backward from num_buf+15, same technique as calc.asm's
    # own itoa -- handles a sign (subtraction can go negative, e.g.
    # 2-9=-7) and up to a few digits (multiplication up to 9*9=81).
    num_buf_end = num_buf_addr + 15

    # lea rdi, [num_buf_end]  (RDI = fixed end cursor of the buffer)
    # Simpler here: just keep track of the write cursor in RCX,
    # computed via immediates rather than a runtime lea, since
    # num_buf_end is a compile-time-known constant address.
    # mov rcx, num_buf_end
    c += bytes([0x48, 0xC7, 0xC1]) + struct.pack("<i", num_buf_end if num_buf_end < 0x80000000 else num_buf_end - (1 << 32))

    # ebx = 0/1 sign flag
    # xor ebx, ebx
    c += bytes([0x31, 0xDB])
    # test eax, eax
    c += bytes([0x85, 0xC0])
    jns_pos = len(c)
    c += bytes([0x79, 0x00])
    # mov ebx, 1
    c += bytes([0xBB]) + struct.pack("<I", 1)
    # neg eax
    c += bytes([0xF7, 0xD8])
    jns_target = len(c)
    c[jns_pos + 1] = (jns_target - (jns_pos + 2)) & 0xFF

    loop_pos = len(c)
    # xor edx, edx
    c += bytes([0x31, 0xD2])
    # mov esi, 10
    c += bytes([0xBE]) + struct.pack("<I", 10)
    # div esi
    c += bytes([0xF7, 0xF6])
    # add dl, '0'
    c += bytes([0x80, 0xC2, ord('0')])
    # dec rcx
    c += bytes([0x48, 0xFF, 0xC9])
    # mov [rcx], dl
    c += bytes([0x88, 0x11])
    # test eax, eax
    c += bytes([0x85, 0xC0])
    jnz_pos = len(c)
    c += bytes([0x75, 0x00])
    c[jnz_pos + 1] = (loop_pos - (jnz_pos + 2)) & 0xFF

    # test ebx, ebx
    c += bytes([0x85, 0xDB])
    jz_pos = len(c)
    c += bytes([0x74, 0x00])
    # dec rcx ; mov byte [rcx], '-'
    c += bytes([0x48, 0xFF, 0xC9])
    c += bytes([0xC6, 0x01, ord('-')])
    jz_target = len(c)
    c[jz_pos + 1] = (jz_target - (jz_pos + 2)) & 0xFF

    # rcx = start of digits. Length = (num_buf_end) - rcx.
    # mov rax, rcx           (RAX = start pointer, becomes SYS_WRITE's buf)
    c += bytes([0x48, 0x89, 0xC8])
    # mov rdx, num_buf_end   (movabs)
    c += bytes([0x48, 0xBA]) + struct.pack("<Q", num_buf_end)
    # sub rdx, rax           (RDX = length)
    c += bytes([0x48, 0x29, 0xC2])

    # mov rsi, rax           (buf for SYS_WRITE)
    c += bytes([0x48, 0x89, 0xC6])
    # mov rax, SYS_WRITE (do this AFTER rsi/rdx are set, rax is clobbered last)
    c += mov_rax_imm32(SYS_WRITE)
    c += mov_rdi_imm32(1)
    c += SYSCALL

    print_msg(msg_newline_addr, msg_newline_len)

    # ── Exit ─────────────────────────────────────────────────────────
    c += mov_rax_imm32(SYS_EXIT)
    c += mov_rdi_imm32(0)
    c += SYSCALL
    c += bytes([0xEB, 0xFE])  # jmp $ -- safety net, never reached

    return bytes(c)


def main():
    out_path = sys.argv[1] if len(sys.argv) > 1 else "calc.axp"

    msg_prompt1 = b"Enter first digit (0-9): "
    msg_prompt_op = b"Enter operator (+, -, *): "
    msg_prompt2 = b"Enter second digit (0-9): "
    msg_equals = b" = "
    msg_newline = b"\n"

    # Two passes: build with placeholder (0) addresses for the
    # messages/scratch area first to learn the code's exact size, then
    # rebuild with the real addresses -- every address is embedded via
    # a fixed-width encoding (movabs = 10 bytes; the [disp32] absolute
    # addressing used for key_buf/operand/operator/num_buf accesses is
    # also always the same width regardless of the address's value),
    # so the code's length is identical between passes. Same technique
    # make_test_axp.py and make_test_driver_axp.py already use, applied
    # here across more addresses.
    dummy = build_code(0, len(msg_prompt1), 0, len(msg_prompt_op),
                        0, len(msg_prompt2), 0, len(msg_equals),
                        0, len(msg_newline), 0, 0)
    code_size = len(dummy)

    text_data_size_estimate = code_size + len(msg_prompt1) + len(msg_prompt_op) + \
        len(msg_prompt2) + len(msg_equals) + len(msg_newline)

    msg_prompt1_addr = BASE_ADDR + code_size
    msg_prompt_op_addr = msg_prompt1_addr + len(msg_prompt1)
    msg_prompt2_addr = msg_prompt_op_addr + len(msg_prompt_op)
    msg_equals_addr = msg_prompt2_addr + len(msg_prompt2)
    msg_newline_addr = msg_equals_addr + len(msg_equals)

    # Scratch area (key_buf + operand_a + operand_b + operator, 4 bytes,
    # then num_buf, 16 bytes) placed right after the last message --
    # this .axp's single RWX section covers this range too (see
    # make_test_axp.py's own comment on why RWX in V1: simplest option,
    # no separate .bss section support needed for a single-section
    # program), so there's real backing memory there even though
    # nothing is ever loaded from disk into it (BASE_ADDR's section
    # covers the whole mem_size, file_size only covers up to the actual
    # on-disk content -- the loader zero-fills the difference, see
    # Formats/AXF/AXP/SectionLoader.c).
    key_buf_addr = msg_newline_addr + len(msg_newline)
    # 4 bytes: key_buf_addr(1)/operand_a(1)/operand_b(1)/operator(1),
    # then 4 bytes for result_addr (computed inside build_code() from
    # key_buf_addr, same as operand_a_addr/operand_b_addr/
    # operator_addr are), then num_buf (16 bytes).
    num_buf_addr = key_buf_addr + 4 + 4
    scratch_size = 4 + 4 + 16

    code = build_code(msg_prompt1_addr, len(msg_prompt1),
                       msg_prompt_op_addr, len(msg_prompt_op),
                       msg_prompt2_addr, len(msg_prompt2),
                       msg_equals_addr, len(msg_equals),
                       msg_newline_addr, len(msg_newline),
                       key_buf_addr, num_buf_addr)
    assert len(code) == code_size, f"code size changed between passes: {code_size} != {len(code)}"

    text_data = code + msg_prompt1 + msg_prompt_op + msg_prompt2 + msg_equals + msg_newline
    file_size = len(text_data)  # on-disk portion -- scratch area is NOT written to disk
    mem_size = file_size + scratch_size  # in-memory portion includes the zero-filled scratch area

    entry_point = BASE_ADDR

    header_size = 64
    section_size = 56
    section_table_offset = header_size
    text_file_offset = section_table_offset + section_size

    header = struct.pack(
        "<IHHQQII4Q",
        AXF_MAGIC,
        AXF_VERSION,
        AXF_TYPE_AXP,
        entry_point,
        BASE_ADDR,
        1,  # section_count
        section_table_offset,
        0, 0, 0, 0,  # export_count, export_table_offset, import_count, import_table_offset (all unused, packed into the same 4Q as reserved[2] would be -- matches make_test_axp.py's own header packing exactly)
    )
    assert len(header) == header_size, f"header size mismatch: {len(header)}"

    name = b".text" + b"\x00" * (16 - len(b".text"))
    section = struct.pack(
        "<16sQQQQII",
        name,
        BASE_ADDR,
        mem_size,
        text_file_offset,
        file_size,
        R | W | X,
        0,
    )
    assert len(section) == section_size, f"section size mismatch: {len(section)}"

    with open(out_path, "wb") as f:
        f.write(header)
        f.write(section)
        f.write(text_data)

    total = header_size + section_size + file_size
    print(f"Écrit {out_path} ({total} octets sur disque, {mem_size} octets en mémoire)")
    print(f"  entry_point = {hex(entry_point)}")
    print(f"  code = {code_size} octets, messages @ {hex(msg_prompt1_addr)}")
    print(f"  scratch (key_buf/operands/operator/num_buf) @ {hex(key_buf_addr)}, {scratch_size} octets (zero-filled, not on disk)")


if __name__ == "__main__":
    main()
