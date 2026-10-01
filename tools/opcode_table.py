#!/usr/bin/env python3
"""
opcode_table.py - generate the Game Boy opcode map as data, not as 512 guesses.

Run it:

    python tools/opcode_table.py                 # print a summary + the base table
    python tools/opcode_table.py --markdown      # print the full Markdown tables
    python tools/opcode_table.py --out opcodes.md
    python tools/opcode_table.py --check gb/src/opcodes.c

Why you want this in M02-M04:

  * The opcode map is not 512 facts, it is about 40 facts plus a lot of
    regularity. This file encodes the REGULARITY (which never contains a typo)
    and keeps the IRREGULAR entries as data you can eyeball in one screen.
  * `--check` greps your C source for each mnemonic. Every "missing" line is an
    opcode you forgot, found mechanically instead of by running a game and
    watching it hang.
  * It is also the fastest way to build the mnemonic column of a table-driven
    `opcodes.c`, and the table you need for the M11 disassembler.

Reference data in IRREGULAR was cross-checked against gbctr's opcode tables and
`gb-opcodes`' Opcodes.json, both vendored in reference/tables/. Cycle counts are
T-cycles and INCLUDE the 4 T-cycles of the 0xCB prefix fetch (see
reference/cheatsheet-flags-and-timing.md for the other convention).

Exercise ideas, in increasing difficulty:
  1. Emit your C table as a `static const struct { const char *mnem; u8 bytes;
     u8 cycles; u8 cycles_taken; }` literal, ready to paste.
  2. Emit a `{op, mnemonic, bytes, cycles}` CSV and write a C test that walks
     your real table and compares it entry by entry. That test is worth more
     than any screenshot.
  3. Cross-check the cycles column against reference/tables/gbctr-opcodes.toml
     and fail loudly on a mismatch.
"""

import argparse
import os
import re
import sys

REG8 = ["B", "C", "D", "E", "H", "L", "(HL)", "A"]
ALU = ["ADD", "ADC", "SUB", "SBC", "AND", "XOR", "OR", "CP"]
CB_OPS = ["RLC", "RRC", "RL", "RR", "SLA", "SRA", "SWAP", "SRL"]

# Opcodes on which the real CPU locks up. Reaching one in an emulator means your
# decode already went wrong, so treat it as a guest fault (cpu_fatal).
ILLEGAL = [0xD3, 0xDB, 0xDD, 0xE3, 0xE4, 0xEB, 0xEC, 0xED, 0xF4, 0xFC, 0xFD]

# Irregular opcodes: mnemonic, byte length, T-cycles, T-cycles when a condition
# is taken (0 = unconditional). Everything not in here is generated.
IRREGULAR = {
    0x00: ("NOP", 1, 4, 0),
    0x01: ("LD BC,d16", 3, 12, 0), 0x11: ("LD DE,d16", 3, 12, 0),
    0x21: ("LD HL,d16", 3, 12, 0), 0x31: ("LD SP,d16", 3, 12, 0),
    0x02: ("LD (BC),A", 1, 8, 0), 0x12: ("LD (DE),A", 1, 8, 0),
    0x0A: ("LD A,(BC)", 1, 8, 0), 0x1A: ("LD A,(DE)", 1, 8, 0),
    0x22: ("LD (HL+),A", 1, 8, 0), 0x2A: ("LD A,(HL+)", 1, 8, 0),
    0x32: ("LD (HL-),A", 1, 8, 0), 0x3A: ("LD A,(HL-)", 1, 8, 0),
    0x03: ("INC BC", 1, 8, 0), 0x13: ("INC DE", 1, 8, 0),
    0x23: ("INC HL", 1, 8, 0), 0x33: ("INC SP", 1, 8, 0),
    0x0B: ("DEC BC", 1, 8, 0), 0x1B: ("DEC DE", 1, 8, 0),
    0x2B: ("DEC HL", 1, 8, 0), 0x3B: ("DEC SP", 1, 8, 0),
    0x04: ("INC B", 1, 4, 0), 0x0C: ("INC C", 1, 4, 0),
    0x14: ("INC D", 1, 4, 0), 0x1C: ("INC E", 1, 4, 0),
    0x24: ("INC H", 1, 4, 0), 0x2C: ("INC L", 1, 4, 0),
    0x34: ("INC (HL)", 1, 12, 0), 0x3C: ("INC A", 1, 4, 0),
    0x05: ("DEC B", 1, 4, 0), 0x0D: ("DEC C", 1, 4, 0),
    0x15: ("DEC D", 1, 4, 0), 0x1D: ("DEC E", 1, 4, 0),
    0x25: ("DEC H", 1, 4, 0), 0x2D: ("DEC L", 1, 4, 0),
    0x35: ("DEC (HL)", 1, 12, 0), 0x3D: ("DEC A", 1, 4, 0),
    0x06: ("LD B,d8", 2, 8, 0), 0x0E: ("LD C,d8", 2, 8, 0),
    0x16: ("LD D,d8", 2, 8, 0), 0x1E: ("LD E,d8", 2, 8, 0),
    0x26: ("LD H,d8", 2, 8, 0), 0x2E: ("LD L,d8", 2, 8, 0),
    0x36: ("LD (HL),d8", 2, 12, 0), 0x3E: ("LD A,d8", 2, 8, 0),
    0x07: ("RLCA", 1, 4, 0), 0x0F: ("RRCA", 1, 4, 0),
    0x17: ("RLA", 1, 4, 0), 0x1F: ("RRA", 1, 4, 0),
    0x08: ("LD (a16),SP", 3, 20, 0),
    0xCB: ("PREFIX CB", 1, 4, 0),
    0x09: ("ADD HL,BC", 1, 8, 0), 0x19: ("ADD HL,DE", 1, 8, 0),
    0x29: ("ADD HL,HL", 1, 8, 0), 0x39: ("ADD HL,SP", 1, 8, 0),
    0x10: ("STOP", 2, 4, 0), 0x18: ("JR e8", 2, 12, 0),
    0x20: ("JR NZ,e8", 2, 8, 12), 0x28: ("JR Z,e8", 2, 8, 12),
    0x30: ("JR NC,e8", 2, 8, 12), 0x38: ("JR C,e8", 2, 8, 12),
    0x27: ("DAA", 1, 4, 0), 0x2F: ("CPL", 1, 4, 0),
    0x37: ("SCF", 1, 4, 0), 0x3F: ("CCF", 1, 4, 0),
    0xC0: ("RET NZ", 1, 8, 20), 0xC8: ("RET Z", 1, 8, 20),
    0xD0: ("RET NC", 1, 8, 20), 0xD8: ("RET C", 1, 8, 20),
    0xC1: ("POP BC", 1, 12, 0), 0xD1: ("POP DE", 1, 12, 0),
    0xE1: ("POP HL", 1, 12, 0), 0xF1: ("POP AF", 1, 12, 0),
    0xC2: ("JP NZ,a16", 3, 12, 16), 0xCA: ("JP Z,a16", 3, 12, 16),
    0xD2: ("JP NC,a16", 3, 12, 16), 0xDA: ("JP C,a16", 3, 12, 16),
    0xC3: ("JP a16", 3, 16, 0),
    0xC4: ("CALL NZ,a16", 3, 12, 24), 0xCC: ("CALL Z,a16", 3, 12, 24),
    0xD4: ("CALL NC,a16", 3, 12, 24), 0xDC: ("CALL C,a16", 3, 12, 24),
    0xC5: ("PUSH BC", 1, 16, 0), 0xD5: ("PUSH DE", 1, 16, 0),
    0xE5: ("PUSH HL", 1, 16, 0), 0xF5: ("PUSH AF", 1, 16, 0),
    0xC6: ("ADD A,d8", 2, 8, 0), 0xCE: ("ADC A,d8", 2, 8, 0),
    0xD6: ("SUB d8", 2, 8, 0), 0xDE: ("SBC A,d8", 2, 8, 0),
    0xE6: ("AND d8", 2, 8, 0), 0xEE: ("XOR d8", 2, 8, 0),
    0xF6: ("OR d8", 2, 8, 0), 0xFE: ("CP d8", 2, 8, 0),
    0xC7: ("RST 00H", 1, 16, 0), 0xCF: ("RST 08H", 1, 16, 0),
    0xD7: ("RST 10H", 1, 16, 0), 0xDF: ("RST 18H", 1, 16, 0),
    0xE7: ("RST 20H", 1, 16, 0), 0xEF: ("RST 28H", 1, 16, 0),
    0xF7: ("RST 30H", 1, 16, 0), 0xFF: ("RST 38H", 1, 16, 0),
    0xC9: ("RET", 1, 16, 0), 0xD9: ("RETI", 1, 16, 0),
    0xCD: ("CALL a16", 3, 24, 0),
    0xE0: ("LDH (a8),A", 2, 12, 0), 0xF0: ("LDH A,(a8)", 2, 12, 0),
    0xE2: ("LD (C),A", 1, 8, 0), 0xF2: ("LD A,(C)", 1, 8, 0),
    0xE8: ("ADD SP,e8", 2, 16, 0), 0xF8: ("LD HL,SP+e8", 2, 12, 0),
    0xE9: ("JP HL", 1, 4, 0), 0xF9: ("LD SP,HL", 1, 8, 0),
    0xEA: ("LD (a16),A", 3, 16, 0), 0xFA: ("LD A,(a16)", 3, 16, 0),
    0xF3: ("DI", 1, 4, 0), 0xFB: ("EI", 1, 4, 0),
}


def base_entry(op):
    """Return (mnemonic, bytes, cycles, cycles_taken, is_illegal) for a base opcode."""
    if op in ILLEGAL:
        return ("illegal", 1, 0, 0, True)
    if op in IRREGULAR:
        m, n, c, t = IRREGULAR[op]
        return (m, n, c, t, False)
    if 0x40 <= op <= 0x7F:
        dst, src = (op >> 3) & 7, op & 7
        if dst == 6 and src == 6:
            return ("HALT", 1, 4, 0, False)
        cycles = 8 if (dst == 6 or src == 6) else 4
        return ("LD %s,%s" % (REG8[dst], REG8[src]), 1, cycles, 0, False)
    if 0x80 <= op <= 0xBF:
        name, src = ALU[(op - 0x80) >> 3], REG8[op & 7]
        cycles = 8 if src == "(HL)" else 4
        operand = "d8" if src == "(HL)" else src
        return ("%s A,%s" % (name, operand), 2 if src == "(HL)" else 1, cycles, 0, False)
    raise AssertionError("unhandled base opcode 0x%02X" % op)


def cb_entry(op):
    """Return (mnemonic, bytes, cycles) for a CB-prefixed opcode (prefix included).

    This is the regularity the whole page is built from:
        0x00-0x3F  CB_OPS[(op>>3)&7] on REG8[op&7]
        0x40-0x7F  BIT n,r      with n=(op>>3)&7, r=op&7
        0x80-0xBF  RES n,r
        0xC0-0xFF  SET n,r
    """
    reg = REG8[op & 7]
    n = (op >> 3) & 7
    if op < 0x40:
        cycles = 16 if reg == "(HL)" else 8
        return ("%s %s" % (CB_OPS[(op >> 3) & 7], reg), 2, cycles, 0)
    if op < 0x80:
        cycles = 12 if reg == "(HL)" else 8
        return ("BIT %d,%s" % (n, reg), 2, cycles, 0)
    cycles = 16 if reg == "(HL)" else 8
    if op < 0xC0:
        return ("RES %d,%s" % (n, reg), 2, cycles, 0)
    return ("SET %d,%s" % (n, reg), 2, cycles, 0)


def build():
    base = {op: base_entry(op) for op in range(256)}
    cb = {op: cb_entry(op) for op in range(256)}
    return base, cb


def cycles_text(cycles, taken):
    return "%d/%d" % (cycles, taken) if taken else str(cycles)


def print_summary(base, cb):
    illegal = [op for op, e in base.items() if e[4]]
    print("base opcodes      : 256")
    print("legally reachable : %d" % (256 - len(illegal)))
    print("illegal (cpu_fatal): %s" % " ".join("%02X" % o for o in illegal))
    print("CB opcodes        : %d" % len(cb))
    bytes_hist = {n: sum(1 for e in base.values() if e[1] == n) for n in (1, 2, 3)}
    print("byte lengths      : 1-byte %d, 2-byte %d, 3-byte %d"
          % (bytes_hist[1], bytes_hist[2], bytes_hist[3]))
    print()
    print("Spot checks (memorise these; if they disagree with your C table, one of")
    print("the two is wrong and it is worth finding out which):")
    for op in (0xCB, 0x08, 0x36, 0x76, 0xE8, 0xF8, 0xCD, 0xC9):
        if op == 0xCB:
            print("  CB 00            -> %-14s %d T" % (cb[0x00][0], cb[0x00][2]))
            print("  CB 06            -> %-14s %d T" % (cb[0x06][0], cb[0x06][2]))
            print("  CB 7E            -> %-14s %d T" % (cb[0x7E][0], cb[0x7E][2]))
            continue
        e = base[op]
        print("  %02X               -> %-14s %d bytes, %s T"
              % (op, e[0], e[1], cycles_text(e[2], e[3])))


def markdown(base, cb):
    lines = ["# Generated opcode map", "",
             "Produced by `tools/opcode_table.py`. Cycles are T-cycles and include",
             "the 4 T-cycles of the CB prefix fetch.", ""]
    for label, table in (("Base opcodes", base), ("CB-prefixed opcodes", cb)):
        lines += ["## %s" % label, "",
                  "| Op | Mnemonic | Bytes | T-cycles |", "| --- | --- | --- | --- |"]
        for op in range(256):
            e = table[op]
            lines.append("| `%02X` | %s | %d | %s |"
                         % (op, e[0], e[1], cycles_text(e[2], e[3])))
        lines.append("")
    return "\n".join(lines)


def check(path, base, cb):
    try:
        with open(path, "r", encoding="utf-8", errors="replace") as fh:
            source = fh.read()
    except OSError as exc:
        print("cannot read %s: %s" % (path, exc), file=sys.stderr)
        return 2

    missing = []
    for op, e in sorted(base.items()):
        name = e[0]
        if e[4]:
            continue
        # Look for the mnemonic or its opcode byte in the source. This is a
        # blunt instrument on purpose: it finds omissions, not correctness.
        token = name.split()[0]
        has_mnemonic = token in source
        has_opcode = re.search("0x" + ("%02X" % op), source, re.IGNORECASE) is not None
        if not has_mnemonic and not has_opcode:
            missing.append((op, name))

    if missing:
        print("%d base opcodes with no obvious trace in %s:" % (len(missing), path))
        for op, name in missing[:60]:
            print("  %02X  %s" % (op, name))
        print("\n(%d total; see reference/cheatsheet-opcode-map.md)" % len(missing))
    else:
        print("%s mentions every reachable base opcode. Not a proof, but a good sign."
              % path)
    print("CB page: %d entries generated." % len(cb))
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[1])
    ap.add_argument("--markdown", action="store_true", help="print the full tables")
    ap.add_argument("--out", metavar="FILE", help="write the Markdown tables to FILE")
    ap.add_argument("--check", metavar="C_SOURCE",
                    help="report base opcodes absent from your C source")
    args = ap.parse_args()

    base, cb = build()

    if args.check:
        return check(args.check, base, cb)
    if args.out:
        out_dir = os.path.dirname(os.path.abspath(args.out))
        if out_dir and not os.path.isdir(out_dir):
            os.makedirs(out_dir, exist_ok=True)
        with open(args.out, "w", encoding="utf-8", newline="\n") as fh:
            fh.write(markdown(base, cb))
        print("wrote %s" % args.out)
        return 0
    if args.markdown:
        print(markdown(base, cb))
        return 0

    print_summary(base, cb)
    return 0


if __name__ == "__main__":
    sys.exit(main())
