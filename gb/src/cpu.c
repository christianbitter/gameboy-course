/*
 * cpu.c - registers, fetch, dispatch. YOURS TO IMPLEMENT.
 *
 * Milestones:
 *   M02  fetch/decode loop, registers, r8 helpers, loud abort on unknown ops
 *   M05  interrupt dispatch, EI delay, HALT
 *   M06  gb_apply_post_boot_state()
 *
 * Split with opcodes.c: cpu_step() owns "what happens between instructions"
 * (interrupts, HALT, fetch); op_execute() owns "what one opcode does".
 * If you would rather have one giant switch, put it in op_execute() and keep
 * cpu_step() as the loop. The split is for readability, not dogma.
 */
#include "gb/gb.h"
#include <stdarg.h>

/* ------------------------------------------------------------------------- */
/* PROVIDED                                                                  */
/* ------------------------------------------------------------------------- */

void cpu_reset(gb_t *gb)
{
    memset(&gb->cpu, 0, sizeof gb->cpu);
    gb->cpu.pc = 0x0100;
    /* Add anything your implementation needs cleared on reset. */
}

/*
 * Report a guest fault: something the hardware cannot do (the 11 illegal
 * opcodes, an impossible state). This is a legal, testable outcome: it sets
 * gb->fatal, dumps the tracer, and every later gb_step() becomes a no-op.
 *
 * Do NOT use this for "I have not written this code yet" - that is
 * GB_UNIMPLEMENTED(), which means a bug in your emulator.
 */
void cpu_fatal(gb_t *gb, const char *fmt, ...)
{
    char what[256];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(what, sizeof what, fmt, ap);
    va_end(ap);

    fprintf(stderr, "\n[gb] CPU FAULT: %s\n", what);
    fprintf(stderr,
            "     PC=%04X opcode=%02X len=%u SP=%04X "
            "AF=%02X%02X BC=%02X%02X DE=%02X%02X HL=%02X%02X IME=%d\n",
            gb->cpu.pc, gb->cpu.last_opcode, gb->cpu.last_opcode_len, gb->cpu.sp,
            gb->cpu.a, gb->cpu.f, gb->cpu.b, gb->cpu.c,
            gb->cpu.d, gb->cpu.e, gb->cpu.h, gb->cpu.l, (int)gb->cpu.ime);
    fputs("     last instructions:\n", stderr);
    gb_trace_print_last(gb, stderr, 12);

    gb->fatal = true;
}

/* ------------------------------------------------------------------------- */
/* TODO(M02) - the fetch/decode loop                                         */
/* ------------------------------------------------------------------------- */

/*
 * Fetch one byte from (PC) and advance PC. PC semantics matter:
 *   - PC points at the NEXT byte to fetch before you call this.
 *   - After this returns, PC points at the following byte, so any opcode that
 *     reads 1 or 2 operand bytes is one line shorter than you expect.
 */
u8 cpu_fetch8(gb_t *gb)
{
    (void)gb;
    GB_UNIMPLEMENTED("cpu_fetch8() - M02: read the byte at PC and post-increment PC");
    return 0xFF;
}

/*
 * Fetch a little-endian 16-bit immediate: low byte first, then high byte.
 * Do not read it as a single 16-bit bus access - see docs/03 section 3.2.
 */
u16 cpu_fetch16(gb_t *gb)
{
    (void)gb;
    GB_UNIMPLEMENTED("cpu_fetch16() - M02: low byte then high byte");
    return 0xFFFF;
}

/*
 * The r8 operand space: idx 0..7 = B C D E H L (HL) A.
 *
 * Index 6 is the special one: it reads/writes memory at HL through the bus.
 * Writing these two functions well collapses the 0x40-0x7F and 0x80-0xBF
 * blocks from 128 functions into two loops of eight.
 */
u8 cpu_read_r8(gb_t *gb, int idx)
{
    (void)gb;
    GB_UNIMPLEMENTED("cpu_read_r8(idx=%d) - M02", idx);
    return 0xFF;
}

void cpu_write_r8(gb_t *gb, int idx, u8 value)
{
    (void)gb; (void)value;
    GB_UNIMPLEMENTED("cpu_write_r8(idx=%d) - M02", idx);
}

/*
 * Execute ONE step of the machine: dispatch a pending interrupt, or fetch and
 * execute one instruction. Returns the T-cycles consumed, and must set
 *   gb->cpu.last_opcode      (the raw opcode byte)
 *   gb->cpu.last_opcode_len  (1, 2 or 3)
 * because the tracer reads them.
 *
 * Order of business:
 *   1. if (gb->fatal) return 0;
 *   2. resolve a pending EI (gb->cpu.ime_pending) from the previous step;
 *   3. if IME and (IE & IF & 0x1F), dispatch the interrupt and return 20;
 *   4. if halted: do not fetch; wait for IE & IF; return 4;
 *   5. fetch the opcode, call op_execute(), return its cycle count.
 *
 * M09 changes what you return, not where you return it. Once you tick the bus
 * per memory access (bus_tick(4) inside bus_read/bus_write), return only the
 * RESIDUAL cycles the CPU still owes: 0 for an ordinary instruction, 4 for an
 * internal cycle, 20 for an interrupt dispatch. gb_step() keeps calling
 * bus_tick(gb, residual), so the provided code does not change and no time is
 * counted twice.
 */
u32 cpu_step(gb_t *gb)
{
    (void)gb;
    GB_UNIMPLEMENTED("cpu_step() - M02: see the order of business in this comment");
    return 0;
}

/* ------------------------------------------------------------------------- */
/* TODO(M06) - post-boot state                                               */
/* ------------------------------------------------------------------------- */

/*
 * Make the machine look like the boot ROM just jumped to 0x0100, for
 * --no-boot-rom. CPU half: AF=01B0 BC=0013 DE=00D8 HL=014D SP=FFFE PC=0100,
 * IME cleared. I/O half (via bus_write so the owners see it): P1, SC, DIV,
 * TAC, IF, LCDC, STAT, BGP, OBP0, OBP1, IE.
 *
 * The table lives in reference/cheatsheet-flags-and-timing.md. Leave DIV and
 * IF as the table says and be aware they are the two values most likely to
 * differ between sources.
 */
void gb_apply_post_boot_state(gb_t *gb)
{
    (void)gb;
    /* TODO(M06) */
}
