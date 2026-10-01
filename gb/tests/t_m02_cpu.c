/*
 * t_m02_cpu.c - milestone 2: the fetch/decode loop and the first opcodes.
 *
 * Every test here runs cpu_step() directly, so a missing bus_tick() cannot
 * make a CPU bug look like a timing bug.
 */
#include "harness.h"

TEST(m02_nop_advances_pc)
{
    const u8 prog[] = { 0x00, 0x00, 0x00 };
    gb_t *gb = t_exec(prog, sizeof prog, NULL);
    TEST_ASSERT(gb != NULL, "cart_load() failed");

    TEST_EQ(gb->cpu.pc, 0x0101);
    TEST_EQ(gb->cpu.last_opcode, 0x00);
    TEST_EQ(gb->cpu.last_opcode_len, 1);

    t_free(gb);
}

TEST(m02_step_cycles_nop)
{
    const u8 prog[] = { 0x00 };
    gb_t *gb = t_exec(prog, sizeof prog, NULL);
    TEST_ASSERT(gb != NULL, "cart_load() failed");

    TEST_EQ(t_last_cycles, 4);   /* NOP is 1 M-cycle = 4 T-cycles */

    t_free(gb);
}

TEST(m02_ld_r_d8)
{
    const u8 prog[] = { 0x3E, 0x42 };   /* LD A,$42 */
    gb_t *gb = t_exec(prog, sizeof prog, NULL);
    TEST_ASSERT(gb != NULL, "cart_load() failed");

    TEST_EQ(gb->cpu.a, 0x42);
    TEST_EQ(gb->cpu.pc, 0x0102);
    TEST_EQ(gb->cpu.last_opcode_len, 2);
    TEST_EQ(t_last_cycles, 8);          /* 2 M-cycles */

    t_free(gb);
}

TEST(m02_ld_rr_d16)
{
    /* Little endian: low byte first. 01 34 12 is LD BC,$1234, not $3412. */
    const u8 prog[] = { 0x01, 0x34, 0x12 };
    gb_t *gb = t_exec(prog, sizeof prog, NULL);
    TEST_ASSERT(gb != NULL, "cart_load() failed");

    TEST_EQ(gb->cpu.b, 0x12);
    TEST_EQ(gb->cpu.c, 0x34);
    TEST_EQ(cpu_bc(&gb->cpu), 0x1234);
    TEST_EQ(gb->cpu.pc, 0x0103);
    TEST_EQ(t_last_cycles, 12);

    /* 31 34 12 -> LD SP,$1234 */
    const u8 prog2[] = { 0x31, 0x34, 0x12 };
    gb_t *gb2 = t_exec(prog2, sizeof prog2, NULL);
    TEST_ASSERT(gb2 != NULL, "cart_load() failed");
    TEST_EQ(gb2->cpu.sp, 0x1234);
    t_free(gb2);

    t_free(gb);
}

TEST(m02_pc_after_2byte)
{
    /* JP a16 overwrites PC wholesale. */
    const u8 jp[] = { 0xC3, 0x00, 0x20 };
    gb_t *gb = t_exec(jp, sizeof jp, NULL);
    TEST_ASSERT(gb != NULL, "cart_load() failed");
    TEST_EQ(gb->cpu.pc, 0x2000);
    TEST_EQ(t_last_cycles, 16);
    t_free(gb);

    /* LD (a16),A then LD A,(a16) must round-trip through WRAM. */
    cpu_t init = t_cpu_init(0x7700, 0, 0, 0, 0xFFFE);
    const u8 store[] = { 0xEA, 0x00, 0xC0 };
    gb_t *g1 = t_exec(store, sizeof store, &init);
    TEST_ASSERT(g1 != NULL, "cart_load() failed");
    TEST_EQ(bus_read(g1, 0xC000), 0x77);
    TEST_EQ(g1->cpu.pc, 0x0103);
    TEST_EQ(t_last_cycles, 16);
    t_free(g1);

    const u8 load[] = { 0xFA, 0x00, 0xC0 };
    cpu_t empty = t_cpu_init(0x0000, 0, 0, 0, 0xFFFE);
    gb_t *g2 = t_exec(load, sizeof load, &empty);
    TEST_ASSERT(g2 != NULL, "cart_load() failed");
    TEST_EQ(bus_read(g2, 0xC000), 0x00);   /* fresh WRAM is zeroed */
    TEST_EQ(g2->cpu.a, 0x00);
    TEST_EQ(t_last_cycles, 16);
    t_free(g2);
}

TEST(m02_illegal_opcode_traps)
{
    /*
     * 0xD3 is one of the 11 illegal opcodes. It must set gb->fatal through
     * cpu_fatal() - NOT abort the process, and NOT be silently treated as a
     * NOP. This is the difference between a guest fault and a missing feature.
     */
    const u8 prog[] = { 0xD3 };
    gb_t *gb = t_exec(prog, sizeof prog, NULL);
    TEST_ASSERT(gb != NULL, "cart_load() failed");

    TEST_ASSERT(gb->fatal, "executing 0xD3 must call cpu_fatal(), fatal=%d",
                (int)gb->fatal);

    t_free(gb);
}
