/*
 * t_m09_timing.c - access-level timing tests.
 *
 * PROVIDED and real: five tests, no stubs.
 *   L29 (access-level stepping): m09_access_cycle_costs
 *   L30 (the awkward quirks):    m09_timer_overflow_delay, m09_div_apu_edge,
 *                                m09_oam_dma_timing, m09_stat_blocking
 *
 * These are the tests that stop the L29 refactor (moving the clock inside
 * bus_read/bus_write) from silently changing what every earlier lesson measured.
 * m09_access_cycle_costs is deliberately a table of exact instruction totals,
 * including the (HL) forms and the conditional taken/not-taken pairs, because
 * "the instruction totals did not change" is the property that refactor must keep.
 *
 * Two contract notes:
 *   - m09_oam_dma_timing asserts the documented DMG behaviour that the CPU is
 *     STALLED for 160 M-cycles, not merely that 160 bytes get copied. An
 *     implementation that copies instantly without stalling will fail it, which is
 *     intended: see L30.
 *   - m09_div_apu_edge asserts the well-documented half of the divider behaviour
 *     (a DIV write restarts the TIMA clock phase). The exact spurious-tick
 *     injection rules vary between references, so they are deliberately NOT
 *     asserted here; mooneye's timer tests are the oracle for those.
 */
#include "harness.h"

/* ------------------------------------------------------------------------- */
/* L29 - exact instruction totals                                            */
/* ------------------------------------------------------------------------- */

typedef struct {
    u8  bytes[8];
    u8  len;
    u8  steps;      /* how many cpu_step calls to run                     */
    u16 af;
    u16 hl;
    u16 sp;
    u32 want;       /* total T-cycles                                     */
    const char *name;
} cyc_case_t;

/*
 * Elapsed time is measured from the MACHINE's own clock (gb.total_ticks), not by
 * summing cpu_step()'s return value. That is deliberate: L29 moves the clock into the
 * bus, so cpu_step() then returns only the residual internal cycles and the access
 * ticks carry the rest. Reading gb.total_ticks works for both designs, and it fails
 * loudly if the refactor counts time twice (instruction total AND per-access ticks).
 *
 * IE is forced to 0 so no interrupt can fire and add a 20-cycle dispatch.
 */
static void run_cyc_case(const cyc_case_t *tc)
{
    gb_t *gb = t_machine(tc->bytes, tc->len);
    TEST_ASSERT(gb != NULL, "cart_load() failed for %s", tc->name);

    cpu_set_af(&gb->cpu, tc->af);
    cpu_set_hl(&gb->cpu, tc->hl);
    gb->cpu.sp = tc->sp;
    gb->cpu.pc = 0x0100;
    GB_IE(gb) = 0x00;
    GB_IF(gb) = 0x00;
    gb->total_ticks = 0;

    for (int i = 0; i < tc->steps && !gb->fatal; i++) gb_step(gb);

    TEST_ASSERT(gb->total_ticks == tc->want,
                "%s: the machine clock advanced %llu T-cycles, want %u",
                tc->name, (unsigned long long)gb->total_ticks, (unsigned)tc->want);
    t_free(gb);
}

TEST(m09_access_cycle_costs)
{
    static const cyc_case_t cases[] = {
        /* bytes                                  len steps af      hl      sp    want name */
        { { 0x00 },                                 1, 1, 0x0000, 0xC000, 0xFFFE,   4, "NOP" },
        { { 0x3E, 0x42 },                           2, 1, 0x0000, 0xC000, 0xFFFE,   8, "LD A,d8" },
        { { 0x40 },                                 1, 1, 0x0000, 0xC000, 0xFFFE,   4, "LD B,B" },
        { { 0x7E },                                 1, 1, 0x0000, 0xC000, 0xFFFE,   8, "LD A,(HL)" },
        { { 0x36, 0x09 },                           2, 1, 0x0000, 0xC000, 0xFFFE,  12, "LD (HL),d8" },
        { { 0x34 },                                 1, 1, 0x0000, 0xC000, 0xFFFE,  12, "INC (HL)" },
        { { 0x86 },                                 1, 1, 0x0000, 0xC000, 0xFFFE,   8, "ADD A,(HL)" },
        { { 0x01, 0x34, 0x12 },                     3, 1, 0x0000, 0xC000, 0xFFFE,  12, "LD BC,d16" },
        { { 0x03 },                                 1, 1, 0x0000, 0xC000, 0xFFFE,   8, "INC BC" },
        { { 0x09 },                                 1, 1, 0x0000, 0xC000, 0xFFFE,   8, "ADD HL,BC" },
        { { 0xC5 },                                 1, 1, 0x0000, 0xC000, 0xFFFE,  16, "PUSH BC" },
        { { 0xD1 },                                 1, 1, 0x0000, 0xC000, 0xFFFE,  12, "POP DE" },
        { { 0xCD, 0x02, 0x01, 0xC9 },               4, 2, 0x0000, 0xC000, 0xFFFE,  40, "CALL a16 then RET" },
        { { 0xC3, 0x00, 0x20 },                     3, 1, 0x0000, 0xC000, 0xFFFE,  16, "JP a16" },
        { { 0xC2, 0x00, 0x20 },                     3, 1, 0x0000, 0xC000, 0xFFFE,  16, "JP NZ taken" },
        { { 0xC2, 0x00, 0x20 },                     3, 1, 0x8000, 0xC000, 0xFFFE,  12, "JP NZ not taken" },
        { { 0x18, 0x00 },                           2, 1, 0x0000, 0xC000, 0xFFFE,  12, "JR e8" },
        { { 0x20, 0x00 },                           2, 1, 0x0000, 0xC000, 0xFFFE,  12, "JR NZ taken" },
        { { 0x20, 0x00 },                           2, 1, 0x8000, 0xC000, 0xFFFE,   8, "JR NZ not taken" },
        { { 0xC7 },                                 1, 1, 0x0000, 0xC000, 0xFFFE,  16, "RST 00H" },
        { { 0xD9 },                                 1, 1, 0x0000, 0xC000, 0xFFFC,  16, "RETI" },
        { { 0xE9 },                                 1, 1, 0x0000, 0xC000, 0xFFFE,   4, "JP HL" },
        { { 0xF9 },                                 1, 1, 0x0000, 0xC000, 0xFFFE,   8, "LD SP,HL" },
        { { 0xE8, 0x01 },                           2, 1, 0x0000, 0xC000, 0xFFFE,  16, "ADD SP,e8" },
        { { 0xF8, 0x01 },                           2, 1, 0x0000, 0xC000, 0xFFFE,  12, "LD HL,SP+e8" },
        { { 0xE0, 0x00 },                           2, 1, 0x0000, 0xC000, 0xFFFE,  12, "LDH (a8),A" },
        { { 0xF0, 0x00 },                           2, 1, 0x0000, 0xC000, 0xFFFE,  12, "LDH A,(a8)" },
        { { 0xE2 },                                 1, 1, 0x0000, 0xC000, 0xFFFE,   8, "LD (C),A" },
        { { 0xF2 },                                 1, 1, 0x0000, 0xC000, 0xFFFE,   8, "LD A,(C)" },
        { { 0xEA, 0x00, 0xC0 },                     3, 1, 0x0000, 0xC000, 0xFFFE,  16, "LD (a16),A" },
        { { 0xFA, 0x00, 0xC0 },                     3, 1, 0x0000, 0xC000, 0xFFFE,  16, "LD A,(a16)" },
        { { 0x08, 0x00, 0xC0 },                     3, 1, 0x0000, 0xC000, 0xFFFE,  20, "LD (a16),SP" },
        { { 0xC6, 0x01 },                           2, 1, 0x0000, 0xC000, 0xFFFE,   8, "ADD A,d8" },
        { { 0xCE, 0x01 },                           2, 1, 0x0000, 0xC000, 0xFFFE,   8, "ADC A,d8" },
        { { 0x27 },                                 1, 1, 0x0000, 0xC000, 0xFFFE,   4, "DAA" },
        { { 0x07 },                                 1, 1, 0x0000, 0xC000, 0xFFFE,   4, "RLCA" },
        { { 0x2F },                                 1, 1, 0x0000, 0xC000, 0xFFFE,   4, "CPL" },
        { { 0xCB, 0x00 },                           2, 1, 0x0000, 0xC000, 0xFFFE,   8, "CB RLC B" },
        { { 0xCB, 0x06 },                           2, 1, 0x0000, 0xC000, 0xFFFE,  16, "CB RLC (HL)" },
        { { 0xCB, 0x7E },                           2, 1, 0x0000, 0xC000, 0xFFFE,  12, "CB BIT (HL)" },
        { { 0xFB },                                 1, 1, 0x0000, 0xC000, 0xFFFE,   4, "EI" },
        { { 0xF3 },                                 1, 1, 0x0000, 0xC000, 0xFFFE,   4, "DI" },
        { { 0x76 },                                 1, 1, 0x0000, 0xC000, 0xFFFE,   4, "HALT" },

        /* The L12 loop-sum program: 8 bytes, 32 instructions, hand-summed. */
        { { 0x3E, 0x00, 0x06, 0x0A, 0x80, 0x05, 0x20, 0xFC },
                                                    8, 32, 0x0000, 0xC000, 0xFFFE, 212,
          "the L12 loop-sum program, 32 instructions" },
    };

    for (size_t i = 0; i < GB_ARRAY_LEN(cases); i++) run_cyc_case(&cases[i]);
}

/* ------------------------------------------------------------------------- */
/* L30 - the awkward quirks                                                  */
/* ------------------------------------------------------------------------- */

TEST(m09_timer_overflow_delay)
{
    /*
     * On overflow TIMA does not reload immediately: it reads 0x00 for the next
     * 4 T-cycles, and the reload plus the IF bit happen at the end of that window.
     */
    gb_t *gb = t_machine((const u8[]){ 0x00 }, 1);
    TEST_ASSERT(gb != NULL, "cart_load() failed");

    bus_write(gb, 0xFF04, 0x00);          /* reset the divider      */
    bus_write(gb, 0xFF06, 0x42);          /* TMA                    */
    bus_write(gb, 0xFF05, 0xFF);          /* TIMA: one tick away    */
    bus_write(gb, 0xFF07, 0x05);          /* enable, rate 01 = 16 T */
    GB_IF(gb) = 0x00;

    timer_tick(gb, 16);                   /* the falling edge -> overflow */
    TEST_ASSERT(bus_read(gb, 0xFF05) == 0x00,
                "immediately after the overflow TIMA must read 0x00, not the reload "
                "value: got 0x%02X",
                (unsigned)bus_read(gb, 0xFF05));

    timer_tick(gb, 4);
    TEST_ASSERT(bus_read(gb, 0xFF05) == 0x42,
                "4 T-cycles later TIMA must hold TMA (0x42): got 0x%02X",
                (unsigned)bus_read(gb, 0xFF05));
    TEST_ASSERT((GB_IF(gb) & GB_INT_TIMER) != 0,
                "the overflow must request the timer interrupt");

    t_free(gb);
}

TEST(m09_div_apu_edge)
{
    /*
     * DIV is the top byte of the free-running divider, and TIMA is clocked by a bit
     * of that same counter. Writing DIV therefore restarts the TIMA clock's phase,
     * not just the visible DIV value.
     */
    gb_t *gb = t_machine((const u8[]){ 0x00 }, 1);
    TEST_ASSERT(gb != NULL, "cart_load() failed");

    bus_write(gb, 0xFF06, 0x00);          /* TMA */
    bus_write(gb, 0xFF05, 0x00);          /* TIMA */
    bus_write(gb, 0xFF07, 0x05);          /* enable, rate 01 -> period 16 T */
    GB_IF(gb) = 0x00;

    bus_write(gb, 0xFF04, 0x00);          /* reset: the phase starts at 0 */
    timer_tick(gb, 16);
    TEST_ASSERT(bus_read(gb, 0xFF05) == 0x01,
                "with the phase reset the first TIMA tick is exactly 16 T later; "
                "TIMA=0x%02X",
                (unsigned)bus_read(gb, 0xFF05));

    /* Half a period in, then reset the divider: the period must start over. */
    timer_tick(gb, 8);
    bus_write(gb, 0xFF04, 0x00);
    timer_tick(gb, 15);
    TEST_ASSERT(bus_read(gb, 0xFF05) == 0x01,
                "a DIV write must restart the period, so no tick may happen before "
                "16 T have passed; TIMA=0x%02X",
                (unsigned)bus_read(gb, 0xFF05));
    timer_tick(gb, 1);
    TEST_ASSERT(bus_read(gb, 0xFF05) == 0x02,
                "and exactly one tick at 16 T after the DIV write; TIMA=0x%02X",
                (unsigned)bus_read(gb, 0xFF05));

    t_free(gb);
}

TEST(m09_oam_dma_timing)
{
    /*
     * Writing FF46 starts a 160-byte copy to OAM that takes 160 M-cycles (640 T) and
     * stalls the CPU. The test runs gb_step until PC moves past the NOP that follows
     * the FF46 write and requires that to take at least 640 T-cycles.
     *
     * The copied bytes are read from gb->bus.oam rather than through bus_read(0xFE00),
     * because by L30 a correct implementation restricts OAM access during mode 3.
     */
    const u8 prog[] = { 0x3E, 0x80, 0xE0, 0x46, 0x00, 0x00, 0x00, 0x00 };
    gb_t *gb = t_machine(prog, sizeof prog);
    TEST_ASSERT(gb != NULL, "cart_load() failed");

    for (int i = 0; i < 16; i++) bus_write(gb, (u16)(0x8000 + i), (u8)(0xA0 + i));

    gb->cpu.pc = 0x0100;
    gb->total_ticks = 0;
    gb_step(gb);                          /* LD A,0x80        */
    gb_step(gb);                          /* LDH (0x46),A     */
    TEST_ASSERT(gb->cpu.pc == 0x0104,
                "after the write to FF46 the program should be at 0x0104; got 0x%04X",
                gb->cpu.pc);

    u64 start = gb->total_ticks;
    int guard = 0;
    while (gb->cpu.pc == 0x0104 && !gb->fatal && guard++ < 10000) gb_step(gb);
    u64 elapsed = gb->total_ticks - start;

    TEST_ASSERT(elapsed >= 640,
                "the CPU must be stalled for 160 M-cycles (640 T) while OAM DMA "
                "runs; PC moved on after only %llu T",
                (unsigned long long)elapsed);
    TEST_ASSERT(elapsed <= 700,
                "the DMA stall overshot: PC moved on after %llu T",
                (unsigned long long)elapsed);

    for (int i = 0; i < 16; i++) {
        TEST_ASSERT(gb->bus.oam[i] == (u8)(0xA0 + i),
                    "OAM byte %d must have been copied from 0x8000+%d; got 0x%02X",
                    i, i, (unsigned)gb->bus.oam[i]);
    }

    t_free(gb);
}

TEST(m09_stat_blocking)
{
    /*
     * The STAT interrupt is requested on a rising edge of the OR of the enabled
     * sources, so a source that is already high must not keep re-requesting it.
     */
    gb_t *gb = t_machine((const u8[]){ 0x00 }, 1);
    TEST_ASSERT(gb != NULL, "cart_load() failed");

    ppu_reset(gb);
    gb->ppu.lcdc = 0x91;
    gb->ppu.lyc  = 0x00;              /* LY is 0, so the coincidence is true */
    gb->ppu.stat = 0x40;              /* enable the LYC=LY source            */
    GB_IF(gb) = 0x00;

    ppu_tick(gb, 1);
    GB_IF(gb) = 0x00;                 /* ignore whatever the first evaluation did */

    ppu_tick(gb, 200);                /* still line 0: LY == LYC throughout */
    TEST_ASSERT((GB_IF(gb) & GB_INT_STAT) == 0,
                "a STAT source that stays high must not keep re-requesting the "
                "interrupt; IF bit 1 was set again while LY == LYC was still true");

    ppu_tick(gb, 255);                /* 201 + 255 = 456 -> line 1 */
    TEST_EQ(gb->ppu.ly, 1);
    GB_IF(gb) = 0x00;
    gb->ppu.lyc = 0x01;               /* match again -> a new rising edge */
    ppu_tick(gb, 1);
    TEST_ASSERT((GB_IF(gb) & GB_INT_STAT) != 0,
                "once the source goes low and high again, the interrupt must be "
                "requested once more");

    t_free(gb);
}
