/*
 * opcodes.c - the opcode implementations. YOURS TO IMPLEMENT.
 *
 * Milestones:
 *   M02  NOP, LD r,d8, LD rr,d16, JP a16, LD (a16),A, LD A,(a16), illegal ops
 *   M03  0x40-0x7F, 0x80-0xBF, INC/DEC, rotates, CPL/SCF/CCF/DAA, d8 forms
 *   M04  JP/JR cc, CALL/RET/RST, PUSH/POP, 16-bit ops, the whole 0xCB block
 *
 * Two designs both work:
 *
 *  A) a 256-entry table of { mnemonic, bytes, cycles, cycles_taken, fn }, plus
 *     a second 256-entry table for the CB page. The mnemonic in the table is
 *     what makes the M11 disassembler a five-line function, and any hole in the
 *     table shows up immediately as "???".
 *
 *  B) one switch() with a parallel cycle table.
 *
 * Whichever you pick, do not leave the table partly filled: in M02 add a
 * self-check that fails loudly if an entry is missing, and remember that the
 * 11 documented illegal opcodes must fault (see reference/cheatsheet-opcode-map.md).
 */
#include "gb/gb.h"

u32 op_execute(gb_t *gb, u8 opcode)
{
    (void)gb;
    GB_UNIMPLEMENTED("op_execute(opcode=%02X) - see src/opcodes.c for your plan",
                     opcode);
    return 0;
}
