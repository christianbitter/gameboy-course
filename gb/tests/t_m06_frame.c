/*
 * t_m06_frame.c - milestone 6: the frame loop, the PPU skeleton, the joypad,
 * and the post-boot state.
 *
 * Rendering is NOT tested here. A blank screen is acceptable at M06; what must
 * be right is the clock (456 T per line, 154 lines), the VBlank interrupt, the
 * joypad matrix and the post-boot register values.
 */
#include "harness.h"

/*
 * Put the PPU in a known state. Two things are set on purpose:
 *  - LCDC = 0x91 through the bus, because a PPU that only ticks when the LCD is
 *    on must still count here;
 *  - dot/LY/mode are zeroed with ppu_reset(), then LCDC is set again in case
 *    your ppu_reset() clears it.
 */
static void ppu_arm(gb_t *gb)
{
    bus_write(gb, 0xFF40, 0x91);
    ppu_reset(gb);
    bus_write(gb, 0xFF40, 0x91);
}

TEST(m06_frame_is_70224)
{
    gb_t *gb = t_machine((const u8[]){ 0x00 }, 1);
    TEST_ASSERT(gb != NULL, "cart_load() failed");

    TEST_EQ(GB_TICKS_PER_FRAME, 154 * 456);
    TEST_EQ(456 * 144, 65664);            /* dots until VBlank starts */

    ppu_arm(gb);
    ppu_tick(gb, 143 * 456 + 455);        /* one dot short of 144 full lines */
    TEST_ASSERT(!gb->ppu.frame_ready,
                "frame_ready must not be set before line 144 (LY=%u, dot=%u)",
                gb->ppu.ly, gb->ppu.dot);

    ppu_tick(gb, 1);
    TEST_EQ(gb->ppu.ly, 144);
    TEST_EQ(gb->ppu.mode, 1);
    TEST_ASSERT(gb->ppu.frame_ready, "entering VBlank must set frame_ready");

    /* A full frame takes exactly 70224 T-cycles from the top of line 0. */
    ppu_arm(gb);
    ppu_tick(gb, GB_TICKS_PER_FRAME);
    TEST_ASSERT(gb->ppu.frame_ready, "one full frame must have completed");
    TEST_EQ(gb->ppu.ly, 0);
    TEST_EQ(gb->ppu.mode, 2);             /* back to OAM scan on line 0 */

    t_free(gb);
}

TEST(m06_vblank_interrupt_fires)
{
    gb_t *gb = t_machine((const u8[]){ 0x00 }, 1);
    TEST_ASSERT(gb != NULL, "cart_load() failed");

    ppu_arm(gb);
    GB_IF(gb) = 0x00;
    ppu_tick(gb, 143 * 456 + 455);
    TEST_ASSERT(!(GB_IF(gb) & GB_INT_VBLANK),
                "no VBlank interrupt before line 144");

    ppu_tick(gb, 1);
    TEST_ASSERT(GB_IF(gb) & GB_INT_VBLANK, "line 144 must raise IF bit 0");

    /* And the CPU must take it once IME and IE allow. */
    gb->cpu.pc = 0x0100;
    gb->cpu.sp = 0xFFFE;
    gb->cpu.ime = true;
    GB_IE(gb) = GB_INT_VBLANK;
    cpu_step(gb);
    TEST_EQ(gb->cpu.pc, 0x0040);

    t_free(gb);
}

TEST(m06_joypad_matrix)
{
    gb_t *gb = t_machine((const u8[]){ 0x00 }, 1);
    TEST_ASSERT(gb != NULL, "cart_load() failed");

    /* Nothing selected, nothing pressed. */
    bus_write(gb, 0xFF00, 0x30);
    TEST_EQ(bus_read(gb, 0xFF00) & 0x0F, 0x0F);

    /* Directions selected: P14 = 0. Bit 1 is Left. */
    bus_write(gb, 0xFF00, 0x10);
    TEST_EQ(bus_read(gb, 0xFF00) & 0x0F, 0x0F);
    joypad_set(gb, GB_BTN_LEFT, true);
    TEST_EQ(bus_read(gb, 0xFF00) & 0x02, 0x00);
    TEST_EQ(bus_read(gb, 0xFF00) & 0x01, 0x01);   /* Right untouched */
    TEST_EQ(bus_read(gb, 0xFF00) & 0xC0, 0xC0);   /* bits 6-7 always read 1 */
    joypad_set(gb, GB_BTN_LEFT, false);

    /* Buttons selected: P15 = 0. Bit 0 is A. */
    bus_write(gb, 0xFF00, 0x20);
    joypad_set(gb, GB_BTN_A, true);
    TEST_EQ(bus_read(gb, 0xFF00) & 0x01, 0x00);
    joypad_set(gb, GB_BTN_A, false);

    /* A direction key must NOT show up while the button group is selected. */
    joypad_set(gb, GB_BTN_UP, true);
    TEST_EQ(bus_read(gb, 0xFF00) & 0x0F, 0x0F);
    joypad_set(gb, GB_BTN_UP, false);

    /* The interrupt is edge triggered, not level triggered. */
    GB_IF(gb) = 0x00;
    bus_write(gb, 0xFF00, 0x10);
    (void)bus_read(gb, 0xFF00);
    joypad_set(gb, GB_BTN_RIGHT, true);
    (void)bus_read(gb, 0xFF00);
    TEST_ASSERT(GB_IF(gb) & GB_INT_JOYPAD, "a press must raise IF bit 4");

    GB_IF(gb) = 0x00;
    (void)bus_read(gb, 0xFF00);           /* still held: no new edge */
    TEST_ASSERT(!(GB_IF(gb) & GB_INT_JOYPAD),
                "holding a key must not re-trigger the interrupt");

    t_free(gb);
}

TEST(m06_boot_without_bootrom)
{
    /*
     * Skipping the boot ROM is normal and is the default. These are the values
     * the boot ROM leaves behind for the CPU. DIV and IF are deliberately NOT
     * asserted: they depend on how many cycles the boot ROM ran.
     */
    gb_t *gb = t_machine((const u8[]){ 0x00 }, 1);
    TEST_ASSERT(gb != NULL, "cart_load() failed");

    TEST_EQ(cpu_af(&gb->cpu), 0x01B0);
    TEST_EQ(cpu_bc(&gb->cpu), 0x0013);
    TEST_EQ(cpu_de(&gb->cpu), 0x00D8);
    TEST_EQ(cpu_hl(&gb->cpu), 0x014D);
    TEST_EQ(gb->cpu.sp, 0xFFFE);
    TEST_EQ(gb->cpu.pc, 0x0100);
    TEST_EQ(gb->cpu.ime, 0);
    TEST_EQ(gb->ppu.lcdc, 0x91);
    TEST_EQ(GB_IE(gb), 0x00);

    t_free(gb);
}

TEST(m06_run_60_frames)
{
    /* An infinite loop that never touches I/O: 0x18 0xFE is JR -2. */
    const u8 prog[] = { 0x18, 0xFE };
    gb_t *gb = t_machine(prog, sizeof prog);
    TEST_ASSERT(gb != NULL, "cart_load() failed");
    ppu_arm(gb);
    gb->total_ticks = 0;

    for (int i = 0; i < 60; i++) gb_run_frame(gb);

    TEST_ASSERT(!gb->fatal, "the machine faulted while running frames");
    TEST_EQ(gb->frame_count, 60);

    /* Frames are only observed at instruction boundaries, so each one may
     * overshoot by at most one instruction (here 12 T). */
    u64 low = 60ULL * GB_TICKS_PER_FRAME;
    TEST_ASSERT(gb->total_ticks >= low,
                "60 frames took only %llu T-cycles, expected at least %llu",
                (unsigned long long)gb->total_ticks, (unsigned long long)low);
    TEST_ASSERT(gb->total_ticks < low + 60ULL * 32,
                "60 frames took %llu T-cycles, too much overshoot",
                (unsigned long long)gb->total_ticks);

    t_free(gb);
}
