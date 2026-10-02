/*
 * t_m11_polish.c - save states, the disassembler and the tracer.
 *
 * PROVIDED and real: three tests, no stubs. All three are owned by L32/L33.
 *
 *   m11_trace_ring_keeps_last_n  - the tracer itself is provided (src/debug.c), so this
 *                                  one goes green as soon as M01 makes t_machine() work
 *                                  at all; from L33 onward it guards the ring buffer that
 *                                  the tracer and every debugging session rely on.
 *   m11_savestate_roundtrip      - yours: save, mutate everything, load, compare.
 *   m11_disassembler_covers_all_ops - yours: every legal opcode must decode to a
 *                                  mnemonic, not to the raw-hex placeholder.
 *
 * Note on the seam: these tests read and write gb_t's component structs directly
 * (gb->cpu, gb->ppu, gb->timer, gb->bus...). That is not reaching inside - gb_t and
 * the component structs in include/gb/ ARE the agreed public interface of this
 * machine, and every test in the suite uses that seam. Nothing here depends on how
 * the PPU, the timer or the cart are implemented internally, so a rewrite of any of
 * them leaves these tests green.
 */
#include "harness.h"
#include "gb/savestate.h"
#include <ctype.h>

/* A large enough state buffer for any reasonable format; the test asserts gb_save
 * reports its own size and refuses a buffer that is too small. */
static u8 g_state[1 << 20];

/* ------------------------------------------------------------------------- */
/* L33 - the tracer (provided infrastructure, so this is expected to pass)   */
/* ------------------------------------------------------------------------- */

TEST(m11_trace_ring_keeps_last_n)
{
    gb_t *gb = t_machine((const u8[]){ 0x00 }, 1);
    TEST_ASSERT(gb != NULL, "cart_load() failed");

    gb_trace_init(&gb->trace);
    TEST_EQ(gb->trace.count, 0);

    /* Write more entries than the ring can hold. */
    const int written = GB_TRACE_DEPTH + 44;
    for (int i = 0; i < written; i++) {
        gb_trace_record(gb, (u16)(0x0100 + i), 4);
    }

    TEST_ASSERT(gb->trace.count == GB_TRACE_DEPTH,
                "the ring must cap at %d entries; got %u",
                GB_TRACE_DEPTH, (unsigned)gb->trace.count);

    /* The oldest surviving entry must be the 45th written, not the 1st. */
    u32 start = (gb->trace.head + GB_TRACE_DEPTH - gb->trace.count) % GB_TRACE_DEPTH;
    TEST_ASSERT(gb->trace.entries[start].pc == 0x0100 + 44,
                "the ring must drop the OLDEST entries: oldest surviving PC=0x%04X, "
                "expected 0x%04X",
                gb->trace.entries[start].pc, 0x0100 + 44);

    /* The newest entry must be the last one written, with its cycles recorded. */
    u32 last = (gb->trace.head + GB_TRACE_DEPTH - 1) % GB_TRACE_DEPTH;
    TEST_ASSERT(gb->trace.entries[last].pc == 0x0100 + written - 1,
                "the newest entry must be the last written: PC=0x%04X, expected 0x%04X",
                gb->trace.entries[last].pc, 0x0100 + written - 1);
    TEST_EQ(gb->trace.entries[last].cycles, 4);
    TEST_ASSERT(gb->trace.entries[last].len == 1,
                "with no cpu_step the recorded length defaults to 1; got %u",
                (unsigned)gb->trace.entries[last].len);

    t_free(gb);
}

/* ------------------------------------------------------------------------- */
/* L32 - save states                                                         */
/* ------------------------------------------------------------------------- */

TEST(m11_savestate_roundtrip)
{
    const u8 prog[] = { 0x3E, 0x42, 0x06, 0x0A, 0x80, 0x05, 0x20, 0xFC };
    gb_t *gb = t_machine(prog, sizeof prog);
    TEST_ASSERT(gb != NULL, "cart_load() failed");

    /* Build up a state that touches every component, so a forgotten field shows. */
    gb->cpu.pc = 0x0100;
    gb->cpu.sp = 0xFFFE;
    for (int i = 0; i < 5; i++) cpu_step(gb);
    ppu_tick(gb, 456);
    timer_tick(gb, 300);

    bus_write(gb, 0xC123, 0x5A);          /* WRAM            */
    bus_write(gb, 0x8123, 0x3C);          /* VRAM            */
    bus_write(gb, 0xFE05, 0x77);          /* OAM             */
    bus_write(gb, 0xFF80, 0x11);          /* HRAM            */
    bus_write(gb, 0xFF40, 0x91);          /* LCDC            */
    bus_write(gb, 0xFF47, 0xE4);          /* BGP             */
    bus_write(gb, 0xFF06, 0x44);          /* TMA             */
    gb->ppu.scx = 7;
    gb->ppu.scy = 9;
    gb->joypad.pressed = 0x03;
    gb->cpu.f = 0xB0;

    const u32 want_ticks = (u32)gb->total_ticks;
    const u8  want_tima  = gb->timer.tima;      /* captured, not recomputed */

    size_t n = gb_save(gb, g_state, sizeof g_state);
    TEST_ASSERT(n > 0, "gb_save() returned 0 for a 1 MiB buffer");
    TEST_ASSERT(n <= sizeof g_state, "gb_save() claims to have written %u bytes", (unsigned)n);

    TEST_ASSERT(gb_save(gb, g_state, 8) == 0,
                "gb_save() must return 0 when the buffer cannot hold the state");

    /* Snapshot the expectations before mutating. */
    const cpu_t  want_cpu  = gb->cpu;
    const ppu_t  want_ppu  = gb->ppu;
    const u8     want_a    = gb->cpu.a;
    const u16    want_pc   = gb->cpu.pc;

    /* Now scramble everything the state is supposed to restore. */
    memset(&gb->cpu, 0, sizeof gb->cpu);
    memset(&gb->ppu, 0, sizeof gb->ppu);
    memset(gb->bus.vram, 0, sizeof gb->bus.vram);
    memset(gb->bus.wram, 0, sizeof gb->bus.wram);
    memset(gb->bus.oam,  0, sizeof gb->bus.oam);
    memset(gb->bus.hram, 0, sizeof gb->bus.hram);
    gb->timer.tima = 0;
    gb->timer.tma  = 0;
    gb->joypad.pressed = 0;
    gb->total_ticks = 0;

    TEST_ASSERT(gb_load(gb, g_state, n), "gb_load() rejected a state gb_save() produced");

    TEST_EQ(gb->cpu.a, want_a);
    TEST_EQ(gb->cpu.pc, want_pc);
    TEST_EQ(gb->cpu.f, want_cpu.f);
    TEST_EQ(gb->cpu.sp, want_cpu.sp);
    TEST_EQ(gb->total_ticks, want_ticks);

    TEST_EQ(gb->ppu.lcdc, want_ppu.lcdc);
    TEST_EQ(gb->ppu.bgp,  want_ppu.bgp);
    TEST_EQ(gb->ppu.scx,  want_ppu.scx);
    TEST_EQ(gb->ppu.scy,  want_ppu.scy);
    TEST_EQ(gb->ppu.ly,   want_ppu.ly);
    TEST_EQ(gb->ppu.dot,  want_ppu.dot);
    TEST_EQ(gb->ppu.mode, want_ppu.mode);

    TEST_EQ(gb->bus.wram[0x123], 0x5A);
    TEST_EQ(gb->bus.vram[0x123], 0x3C);
    TEST_EQ(gb->bus.oam[5],      0x77);
    TEST_EQ(gb->bus.hram[0],     0x11);
    TEST_EQ(gb->timer.tima, want_tima);     /* captured before the scramble */
    TEST_EQ(gb->timer.tma,  0x44);
    TEST_EQ(gb->joypad.pressed, 0x03);

    /* The framebuffer is part of the picture: a state that does not restore it
     * shows a stale frame for one frame after loading. */
    for (int i = 0; i < GB_FB_SIZE; i += 997) {
        TEST_ASSERT(gb->ppu.framebuffer[i] == want_ppu.framebuffer[i],
                    "framebuffer[%d] was not restored: got %u, want %u",
                    i, (unsigned)gb->ppu.framebuffer[i],
                    (unsigned)want_ppu.framebuffer[i]);
    }

    /* And a foreign buffer must be refused without touching the machine. */
    const u8 before = gb->cpu.a;
    TEST_ASSERT(!gb_load(gb, (const u8[]){ 0xDE, 0xAD, 0xBE, 0xEF }, 4),
                "gb_load() must reject a buffer that is not one of ours");
    TEST_EQ(gb->cpu.a, before);

    t_free(gb);
}

/* ------------------------------------------------------------------------- */
/* L33 - the disassembler                                                    */
/* ------------------------------------------------------------------------- */

static bool is_illegal_op(u8 op)
{
    switch (op) {
    case 0xD3: case 0xDB: case 0xDD: case 0xE3: case 0xE4: case 0xEB:
    case 0xEC: case 0xED: case 0xF4: case 0xFC: case 0xFD:
        return true;
    default:
        return false;
    }
}

TEST(m11_disassembler_covers_all_ops)
{
    /*
     * The contract is deliberately about COVERAGE, not about formatting: for every
     * legal opcode, gb_disasm() must produce a mnemonic. The provided placeholder
     * prints the raw bytes ("7E 00 00"), and a hole prints "???" - so a requirement
     * that the output starts with a LETTER is enough to catch both, while leaving
     * you free to choose spacing, operand syntax, hex vs decimal.
     */
    gb_t *gb = t_machine((const u8[]){ 0x00, 0x00, 0x00, 0x00 }, 4);
    TEST_ASSERT(gb != NULL, "cart_load() failed");

    int checked = 0;
    for (int op = 0; op < 256; op++) {
        if (is_illegal_op((u8)op)) continue;

        gb->cart.rom[0x0100] = (u8)op;
        gb->cart.rom[0x0101] = 0x00;
        gb->cart.rom[0x0102] = 0x00;

        char buf[32];
        memset(buf, 0, sizeof buf);
        gb_disasm(gb, 0x0100, buf);

        TEST_ASSERT(buf[0] != '\0',
                    "opcode 0x%02X disassembled to an empty string", op);
        TEST_ASSERT(isalpha((unsigned char)buf[0]) != 0,
                    "opcode 0x%02X disassembled to \"%s\", which does not start with a "
                    "mnemonic (a hex dump or a ??? hole)", op, buf);
        checked++;
    }

    /* The CB page has its own 256 entry points and must be covered too. */
    for (int cb = 0; cb < 256; cb++) {
        gb->cart.rom[0x0100] = 0xCB;
        gb->cart.rom[0x0101] = (u8)cb;

        char buf[32];
        memset(buf, 0, sizeof buf);
        gb_disasm(gb, 0x0100, buf);

        TEST_ASSERT(isalpha((unsigned char)buf[0]) != 0,
                    "CB 0x%02X disassembled to \"%s\", which does not start with a "
                    "mnemonic", cb, buf);
        checked++;
    }

    TEST_ASSERT(checked == 245 + 256,
                "expected to check 245 legal base opcodes plus 256 CB opcodes; "
                "checked %d", checked);

    /* Spot checks that the mnemonics are the real ones, not placeholders. */
    static const struct { u8 op, cb; const char *want; } spots[] = {
        { 0x00, 0x00, "NOP" },
        { 0x76, 0x00, "HALT" },
        { 0xC3, 0x00, "JP" },
        { 0xCB, 0x00, "RLC" },
        { 0xCB, 0x7E, "BIT" },
        { 0xCB, 0xFF, "SET" },
    };
    for (size_t i = 0; i < GB_ARRAY_LEN(spots); i++) {
        gb->cart.rom[0x0100] = spots[i].op;
        gb->cart.rom[0x0101] = spots[i].cb;
        char buf[32];
        memset(buf, 0, sizeof buf);
        gb_disasm(gb, 0x0100, buf);
        TEST_ASSERT(strstr(buf, spots[i].want) != NULL,
                    "opcode 0x%02X should disassemble to something containing \"%s\"; "
                    "got \"%s\"", spots[i].op, spots[i].want, buf);
    }

    t_free(gb);
}
