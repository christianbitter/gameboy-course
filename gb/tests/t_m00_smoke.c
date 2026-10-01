/*
 * t_m00_smoke.c - milestone 0: the toolchain, the build and the harness.
 *
 * These three tests are GREEN before you write a single line of emulator code.
 * That is deliberate: they prove the loop (edit -> build -> run -> read) works
 * before you have any emulation to blame for a red run.
 */
#include "harness.h"

TEST(m00_smoke_builds)
{
    /* Build works, headers compile, the machine can be created and destroyed. */
    gb_t *gb = gb_create();
    TEST_ASSERT(gb != NULL, "gb_create() returned NULL");
    TEST_EQ(sizeof(gb->ppu.framebuffer), GB_SCREEN_W * GB_SCREEN_H);
    gb_destroy(gb);
}

TEST(m00_types_and_bits)
{
    TEST_EQ(sizeof(u8),  1);
    TEST_EQ(sizeof(s8),  1);
    TEST_EQ(sizeof(u16), 2);
    TEST_EQ(sizeof(u32), 4);
    TEST_EQ(sizeof(u64), 8);

    /* The flag byte: only the top four bits exist. */
    TEST_EQ(GB_FLAG_Z | GB_FLAG_N | GB_FLAG_H | GB_FLAG_C, 0xF0);

    /* Frame arithmetic is worth knowing by heart. */
    TEST_EQ(GB_CYCLES_PER_SCANLINE, 456);
    TEST_EQ(GB_SCANLINES, 154);
    TEST_EQ(GB_TICKS_PER_FRAME, 70224);
    TEST_EQ(GB_CYCLES_PER_SCANLINE * GB_SCANLINES, GB_TICKS_PER_FRAME);

    /* Interrupt vectors. */
    TEST_EQ(GB_INT_VECTOR(0), 0x0040);   /* VBlank  */
    TEST_EQ(GB_INT_VECTOR(1), 0x0048);   /* STAT    */
    TEST_EQ(GB_INT_VECTOR(2), 0x0050);   /* Timer   */
    TEST_EQ(GB_INT_VECTOR(3), 0x0058);   /* Serial  */
    TEST_EQ(GB_INT_VECTOR(4), 0x0060);   /* Joypad  */
}

TEST(m00_harness_selfcheck)
{
    /* The 16-bit accessors are implemented for you; make sure you understand
     * them before you write code that depends on them. */
    cpu_t c;
    memset(&c, 0, sizeof c);

    cpu_set_hl(&c, 0x1234);
    TEST_EQ(c.h, 0x12);
    TEST_EQ(c.l, 0x34);
    TEST_EQ(cpu_hl(&c), 0x1234);

    cpu_set_bc(&c, 0xABCD);
    TEST_EQ(cpu_bc(&c), 0xABCD);
    cpu_set_de(&c, 0x00FF);
    TEST_EQ(cpu_de(&c), 0x00FF);

    /* F: the low nibble does not exist. If this test ever fails, something
     * wrote a raw byte into F instead of going through the flag helpers. */
    cpu_set_af(&c, 0xFFFF);
    TEST_EQ(cpu_af(&c), 0xFFF0);
    TEST_EQ(c.f, 0xF0);

    /* TEST_EQ reports expected vs actual, which is the whole point. */
    TEST_ASSERT(1 == 1, "this should never fire");
}
