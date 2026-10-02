/*
 * t_m07_ppu.c - PPU tests.
 *
 * PROVIDED and real: ten tests, no stubs.
 *   L04 (the scanline clock, BG decode, palettes): m07_lcd_off_blank,
 *       m07_bg_tile_decode, m07_visual_smoke
 *   L21 (sprites):  m07_sprite_priority_x, m07_sprite_10_per_line
 *   L22 (window and scrolling): m07_window_position, m07_bg_scroll_wrap
 *   L23 (STAT and LYC): m07_stat_modes_timing, m07_lyc_coincidence
 *   L25 (raster effects): m07_raster_scroll
 *
 * Style rules that keep these honest:
 *   - gb->ppu fields are set directly instead of through bus_write, so a failure
 *     points at the renderer rather than at the FF40-FF4B routing. VRAM and OAM
 *     are written through the bus, because that is the path a ROM uses.
 *   - Assertions are on FRAMEBUFFER pixels only. No test reaches into internal
 *     renderer state, so any correct implementation passes regardless of how it
 *     is structured.
 *   - The mode-3 length is 172 dots, the convention fixed in L04 and refined in
 *     L30; m07_stat_modes_timing pins it deliberately.
 */
#include "harness.h"

/* A full frame plus one scanline, so a renderer that finishes the last visible
 * line at the mode-3/mode-0 boundary has definitely finished. */
#define TICKS_FULL_FRAME (GB_TICKS_PER_FRAME + GB_CYCLES_PER_SCANLINE)

/* Fill all 8 rows of a tile with the same pair of bitplane bytes. */
static void put_tile_rows(gb_t *gb, u16 tile_addr, u8 lo, u8 hi)
{
    for (int row = 0; row < 8; row++) {
        bus_write(gb, (u16)(tile_addr + row * 2), lo);
        bus_write(gb, (u16)(tile_addr + row * 2 + 1), hi);
    }
}

/* One four-byte OAM entry. Stored Y = screen y + 16, stored X = screen x + 8. */
static void oam_set(gb_t *gb, int index, u8 y, u8 x, u8 tile, u8 attr)
{
    u16 base = (u16)(0xFE00 + index * 4);
    bus_write(gb, base, y);
    bus_write(gb, (u16)(base + 1), x);
    bus_write(gb, (u16)(base + 2), tile);
    bus_write(gb, (u16)(base + 3), attr);
}

static void fill_map(gb_t *gb, u16 base, u8 tile)
{
    for (int i = 0; i < 32 * 32; i++) bus_write(gb, (u16)(base + i), tile);
}

/* A frame's worth of ticks with the LCD on, from a clean PPU. */
static void render_frame(gb_t *gb)
{
    ppu_tick(gb, TICKS_FULL_FRAME);
}

/* ------------------------------------------------------------------------- */
/* L04 - the clock, the BG decode, the palettes                              */
/* ------------------------------------------------------------------------- */

TEST(m07_lcd_off_blank)
{
    gb_t *gb = t_machine((const u8[]){ 0x00 }, 1);
    TEST_ASSERT(gb != NULL, "cart_load() failed");

    /* First prove this VRAM setup really would produce a non-blank screen, so
     * the second half of the test is meaningful instead of vacuous. */
    ppu_reset(gb);
    gb->ppu.lcdc = 0x91;      /* LCD on, BG on, tile data 0x8000, map 0x9800 */
    gb->ppu.bgp  = 0xFF;      /* every colour id -> shade 3 */
    put_tile_rows(gb, 0x8000, 0xFF, 0xFF);
    bus_write(gb, 0x9800, 0x00);

    render_frame(gb);
    TEST_ASSERT(gb->ppu.framebuffer[0] == 3,
                "with the LCD on, this VRAM must render shade 3 at (0,0); got %u",
                (unsigned)gb->ppu.framebuffer[0]);

    /* Now switch the LCD off. Nothing may be drawn, LY stays 0, mode is 0. */
    ppu_reset(gb);
    gb->ppu.lcdc = 0x00;
    gb->ppu.bgp  = 0xFF;
    put_tile_rows(gb, 0x8000, 0xFF, 0xFF);
    bus_write(gb, 0x9800, 0x00);

    render_frame(gb);

    TEST_EQ(gb->ppu.ly, 0);
    TEST_EQ(gb->ppu.mode, 0);
    for (int i = 0; i < GB_FB_SIZE; i++) {
        TEST_ASSERT(gb->ppu.framebuffer[i] == 0,
                    "pixel %d shows shade %u with the LCD off",
                    i, (unsigned)gb->ppu.framebuffer[i]);
    }

    t_free(gb);
}

TEST(m07_bg_tile_decode)
{
    /* lo = 0xAA = 10101010, hi = 0xCC = 11001100, BGP = 0xE4 = identity, so the
     * expected shades are literally the colour ids. */
    static const u8 pattern[8] = { 3, 2, 1, 0, 3, 2, 1, 0 };

    gb_t *gb = t_machine((const u8[]){ 0x00 }, 1);
    TEST_ASSERT(gb != NULL, "cart_load() failed");

    /* --- LCDC.4 = 1: tile data at 0x8000, unsigned indices --- */
    ppu_reset(gb);
    gb->ppu.lcdc = 0x91;
    gb->ppu.bgp  = 0xE4;
    gb->ppu.scx  = 0;
    gb->ppu.scy  = 0;
    put_tile_rows(gb, 0x8000, 0xAA, 0xCC);
    bus_write(gb, 0x9800, 0x00);

    render_frame(gb);

    for (int row = 0; row < 8; row++) {
        for (int x = 0; x < 8; x++) {
            u8 got = gb->ppu.framebuffer[row * GB_SCREEN_W + x];
            TEST_ASSERT(got == pattern[x],
                        "unsigned tile, row %d pixel %d: got shade %u, want %u "
                        "(lo=0xAA, hi=0xCC, BGP=0xE4)",
                        row, x, (unsigned)got, (unsigned)pattern[x]);
        }
    }

    /* --- LCDC.4 = 0: signed indices based at 0x9000, so index 1 -> 0x9010 --- */
    ppu_reset(gb);
    gb->ppu.lcdc = 0x81;      /* LCD on, BG on, LCDC.4 clear */
    gb->ppu.bgp  = 0xE4;
    put_tile_rows(gb, 0x9010, 0x00, 0xFF);   /* every pixel colour id 2 */
    bus_write(gb, 0x9800, 0x01);

    render_frame(gb);

    TEST_ASSERT(gb->ppu.framebuffer[0] == 2,
                "signed tile index 1 must resolve to 0x9010: got shade %u",
                (unsigned)gb->ppu.framebuffer[0]);
    TEST_ASSERT(gb->ppu.framebuffer[7] == 2, "and it must cover all 8 pixels");
    TEST_ASSERT(gb->ppu.framebuffer[GB_SCREEN_W * 7 + 7] == 2, "and all 8 rows");

    t_free(gb);
}

TEST(m07_visual_smoke)
{
    /*
     * Not a precision test - a look-at-it test. It builds five bands, dumps
     * gb/build/l04_frame.bmp, and spot-checks the pixels that prove the bands
     * landed where they should. Open the file after the run.
     */
    gb_t *gb = t_machine((const u8[]){ 0x00 }, 1);
    TEST_ASSERT(gb != NULL, "cart_load() failed");

    ppu_reset(gb);
    gb->ppu.lcdc = 0x91;
    gb->ppu.bgp  = 0xE4;      /* identity */
    gb->ppu.scx  = 0;
    gb->ppu.scy  = 0;

    put_tile_rows(gb, 0x8000, 0x00, 0x00);   /* tile 0: all colour id 0 */
    put_tile_rows(gb, 0x8010, 0xFF, 0x00);   /* tile 1: all colour id 1 */
    put_tile_rows(gb, 0x8020, 0x00, 0xFF);   /* tile 2: all colour id 2 */
    put_tile_rows(gb, 0x8030, 0xFF, 0xFF);   /* tile 3: all colour id 3 */
    put_tile_rows(gb, 0x8040, 0xAA, 0xAA);   /* tile 4: vertical stripes 3/0 */

    /* Map column c shows tile c % 8 on every row. */
    for (int i = 0; i < 32 * 32; i++) {
        bus_write(gb, (u16)(0x9800 + i), (u8)((i % 32) % 8));
    }

    render_frame(gb);

    TEST_ASSERT(gb->ppu.framebuffer[0] == 0,
                "band 0 (x 0-7) should be shade 0, got %u",
                (unsigned)gb->ppu.framebuffer[0]);
    TEST_ASSERT(gb->ppu.framebuffer[8] == 1,
                "band 1 (x 8-15) should be shade 1, got %u",
                (unsigned)gb->ppu.framebuffer[8]);
    TEST_ASSERT(gb->ppu.framebuffer[16] == 2,
                "band 2 (x 16-23) should be shade 2, got %u",
                (unsigned)gb->ppu.framebuffer[16]);
    TEST_ASSERT(gb->ppu.framebuffer[24] == 3,
                "band 3 (x 24-31) should be shade 3, got %u",
                (unsigned)gb->ppu.framebuffer[24]);
    TEST_ASSERT(gb->ppu.framebuffer[32] == 3 && gb->ppu.framebuffer[33] == 0,
                "band 4 (x 32-39) should be stripes 3,0,...; got %u,%u",
                (unsigned)gb->ppu.framebuffer[32],
                (unsigned)gb->ppu.framebuffer[33]);

    gb_write_bmp("gb/build/l04_frame.bmp", ppu_framebuffer(gb));

    t_free(gb);
}

/* ------------------------------------------------------------------------- */
/* L21 - sprites                                                             */
/* ------------------------------------------------------------------------- */

TEST(m07_sprite_priority_x)
{
    /*
     * DMG object priority, in order:
     *   1. only the 10 sprites with the lowest X are candidates (m07_sprite_10_per_line);
     *   2. among those, the LOWEST OAM INDEX wins where they overlap;
     *   3. a sprite pixel with colour id 0 is transparent.
     * Rule 2 is the counter-intuitive one: the naive "draw 0..39 in order" loop
     * leaves the last sprite on top, which is the opposite of the hardware.
     *
     * tile 1 = solid colour id 3, tile 2 = solid colour id 1, tile 0 = id 0.
     */
    gb_t *gb = t_machine((const u8[]){ 0x00 }, 1);
    TEST_ASSERT(gb != NULL, "cart_load() failed");

    ppu_reset(gb);
    gb->ppu.lcdc = 0x93;      /* LCD on, BG on, OBJ on, 8x8, tile data 0x8000 */
    gb->ppu.bgp  = 0xE4;
    gb->ppu.obp0 = 0xE4;
    gb->ppu.scx  = 0;
    gb->ppu.scy  = 0;

    put_tile_rows(gb, 0x8000, 0x00, 0x00);   /* tile 0: colour id 0 (transparent) */
    put_tile_rows(gb, 0x8010, 0xFF, 0xFF);   /* tile 1: colour id 3 */
    put_tile_rows(gb, 0x8020, 0xFF, 0x00);   /* tile 2: colour id 1 */
    fill_map(gb, 0x9800, 0x00);              /* BG is all tile 0 -> shade 0 */

    const int row = 8;
    const u16 r = (u16)(row * GB_SCREEN_W);

    /* --- rule 1/2: two sprites, different X, overlapping --- */
    /* sprite at OAM 0 -> screen x 12..19 ; sprite at OAM 1 -> screen x 16..23 */
    oam_set(gb, 0, 24, 20, 1, 0);
    oam_set(gb, 1, 24, 24, 2, 0);

    render_frame(gb);
    TEST_ASSERT(gb->ppu.framebuffer[r + 12] == 3,
                "OAM 0 sprite (tile 1) should own x=12; got %u",
                (unsigned)gb->ppu.framebuffer[r + 12]);
    TEST_ASSERT(gb->ppu.framebuffer[r + 16] == 3,
                "the overlap (x=16..19) must go to the LOWER X sprite (tile 1), "
                "not to OAM 1 (tile 2); got %u",
                (unsigned)gb->ppu.framebuffer[r + 16]);
    TEST_ASSERT(gb->ppu.framebuffer[r + 20] == 1,
                "x=20 is OAM 1 only, so tile 2's shade 1 must show; got %u",
                (unsigned)gb->ppu.framebuffer[r + 20]);

    /* --- ties go to the lower OAM index --- */
    ppu_reset(gb);
    gb->ppu.lcdc = 0x93;
    gb->ppu.bgp  = 0xE4;
    gb->ppu.obp0 = 0xE4;
    put_tile_rows(gb, 0x8000, 0x00, 0x00);
    put_tile_rows(gb, 0x8010, 0xFF, 0xFF);
    put_tile_rows(gb, 0x8020, 0xFF, 0x00);
    fill_map(gb, 0x9800, 0x00);

    oam_set(gb, 0, 24, 20, 1, 0);   /* screen x 12, tile 1 */
    oam_set(gb, 1, 24, 20, 2, 0);   /* same x, tile 2       */

    render_frame(gb);
    TEST_ASSERT(gb->ppu.framebuffer[r + 12] == 3,
                "with equal X, OAM index 0 must win: expected shade 3, got %u",
                (unsigned)gb->ppu.framebuffer[r + 12]);

    /* --- rule 3: colour id 0 is transparent --- */
    ppu_reset(gb);
    gb->ppu.lcdc = 0x93;
    gb->ppu.bgp  = 0xE4;
    gb->ppu.obp0 = 0xE4;
    put_tile_rows(gb, 0x8000, 0xFF, 0xFF);   /* BG: solid colour id 3 */
    put_tile_rows(gb, 0x8010, 0x00, 0x00);   /* sprite tile: all id 0  */
    fill_map(gb, 0x9800, 0x00);

    oam_set(gb, 0, 24, 20, 1, 0);            /* covers screen x 12..19 */

    render_frame(gb);
    TEST_ASSERT(gb->ppu.framebuffer[r + 12] == 3,
                "a sprite pixel with colour id 0 must be transparent, so the BG "
                "(shade 3) must show through; got %u",
                (unsigned)gb->ppu.framebuffer[r + 12]);

    t_free(gb);
}

TEST(m07_sprite_10_per_line)
{
    /*
     * Only ten objects are drawn per scanline, and the ten are chosen by
     * ASCENDING X (ties by ascending OAM index). Twelve sprites on one line at
     * x = 0, 8, ..., 88 means the two right-most must be dropped.
     */
    gb_t *gb = t_machine((const u8[]){ 0x00 }, 1);
    TEST_ASSERT(gb != NULL, "cart_load() failed");

    ppu_reset(gb);
    gb->ppu.lcdc = 0x93;      /* LCD on, OBJ on, 8x8 */
    gb->ppu.bgp  = 0xE4;
    gb->ppu.obp0 = 0xE4;
    gb->ppu.scx  = 0;
    gb->ppu.scy  = 0;

    put_tile_rows(gb, 0x8000, 0x00, 0x00);   /* BG tile: shade 0 */
    put_tile_rows(gb, 0x8010, 0xFF, 0xFF);   /* sprite tile: shade 3 */
    fill_map(gb, 0x9800, 0x00);

    for (int k = 0; k < 12; k++) {
        /* stored Y 16 -> screen y 0; stored X = screen x + 8 = k*8 + 8 */
        oam_set(gb, k, 16, (u8)(k * 8 + 8), 1, 0);
    }

    render_frame(gb);

    TEST_ASSERT(gb->ppu.framebuffer[0] == 3,
                "sprite 0 (screen x 0) must be drawn; got %u",
                (unsigned)gb->ppu.framebuffer[0]);
    TEST_ASSERT(gb->ppu.framebuffer[72] == 3,
                "sprite 9 (screen x 72) is the 10th by X and must be drawn; got %u",
                (unsigned)gb->ppu.framebuffer[72]);
    TEST_ASSERT(gb->ppu.framebuffer[80] == 0,
                "sprite 10 (screen x 80) is the 11th by X and must be DROPPED "
                "(BG shows instead); got %u",
                (unsigned)gb->ppu.framebuffer[80]);
    TEST_ASSERT(gb->ppu.framebuffer[88] == 0,
                "sprite 11 (screen x 88) must also be dropped; got %u",
                (unsigned)gb->ppu.framebuffer[88]);

    t_free(gb);
}

/* ------------------------------------------------------------------------- */
/* L22 - the window and scrolling                                            */
/* ------------------------------------------------------------------------- */

TEST(m07_bg_scroll_wrap)
{
    /*
     * SCX/SCY index into a 256x256 map that WRAPS. Screen x maps to
     * (x + SCX) & 0xFF, so SCX = 255 shifts every pixel by one and wraps the
     * last map column to the first.
     *
     * tile 0 = shade 0, tile 1 = shade 3, tile 2 = shade 1, BGP identity.
     * Map column 0 = tile 0, column 1 = tile 1, column 31 = tile 2,
     * map row 1 column 0 = tile 1.
     */
    static const struct {
        u8 scx, scy, x, y, want; const char *why;
    } cases[] = {
        {   0,   0,   0, 0, 0, "SCX=0: column 0 is tile 0" },
        {   0,   0,   8, 0, 3, "SCX=0: column 1 is tile 1" },
        {   0,   0, 248, 0, 1, "SCX=0: column 31 is tile 2" },
        {   8,   0,   0, 0, 3, "SCX=8 shifts one whole tile: column 1 now at x=0" },
        {   8,   0,   8, 0, 0, "SCX=8: x=8 is column 2, i.e. tile 0 again" },
        {   4,   0,   0, 0, 0, "SCX=4: x=0 is column 0 pixel 4 -> tile 0" },
        {   4,   0,   4, 0, 3, "SCX=4: x=4 crosses into column 1" },
        { 255,   0,   0, 0, 1, "SCX=255: x=0 is column 31 pixel 7 -> tile 2" },
        { 255,   0,   1, 0, 0, "SCX=255: x=1 wraps back to column 0" },
        {   0,   8,   0, 0, 3, "SCY=8: screen row 0 shows map row 1 -> tile 1" },
        {   0,   8,   0, 8, 0, "SCY=8: screen row 8 is map row 2 -> tile 0" },
    };

    for (size_t i = 0; i < GB_ARRAY_LEN(cases); i++) {
        gb_t *gb = t_machine((const u8[]){ 0x00 }, 1);
        TEST_ASSERT(gb != NULL, "cart_load() failed");

        ppu_reset(gb);
        gb->ppu.lcdc = 0x91;
        gb->ppu.bgp  = 0xE4;
        gb->ppu.scx  = cases[i].scx;
        gb->ppu.scy  = cases[i].scy;

        put_tile_rows(gb, 0x8000, 0x00, 0x00);   /* tile 0 -> shade 0 */
        put_tile_rows(gb, 0x8010, 0xFF, 0xFF);   /* tile 1 -> shade 3 */
        put_tile_rows(gb, 0x8020, 0xFF, 0x00);   /* tile 2 -> shade 1 */

        fill_map(gb, 0x9800, 0x00);
        bus_write(gb, 0x9800 + 1, 0x01);         /* map row 0, column 1  */
        bus_write(gb, 0x9800 + 31, 0x02);        /* map row 0, column 31 */
        bus_write(gb, 0x9800 + 32, 0x01);        /* map row 1, column 0  */

        render_frame(gb);

        u16 idx = (u16)(cases[i].y * GB_SCREEN_W + cases[i].x);
        u8 got = gb->ppu.framebuffer[idx];
        TEST_ASSERT(got == cases[i].want,
                    "SCX=%u SCY=%u pixel (%u,%u): %s; got shade %u, want %u",
                    cases[i].scx, cases[i].scy, cases[i].x, cases[i].y,
                    cases[i].why, (unsigned)got, (unsigned)cases[i].want);

        t_free(gb);
    }
}

TEST(m07_window_position)
{
    /*
     * The window is a second background layer that starts at (WX-7, WY) and has
     * its OWN line counter, which advances only on lines where the window was
     * drawn. Using LY as the window's row is the classic bug; case D below is
     * built specifically to catch it.
     *
     * BG map 0x9800 -> tile 0 (shade 0). Window map 0x9C00 -> tile 1 (shade 3).
     * LCDC = 0xF1: LCD on, window map 0x9C00, window on, tile data 0x8000, BG on.
     */
    const u8 lcdc = 0xF1;

    /* --- case A: the window covers everything --- */
    {
        gb_t *gb = t_machine((const u8[]){ 0x00 }, 1);
        TEST_ASSERT(gb != NULL, "cart_load() failed");
        ppu_reset(gb);
        gb->ppu.lcdc = lcdc;
        gb->ppu.bgp  = 0xE4;
        gb->ppu.wy   = 0;
        gb->ppu.wx   = 7;                    /* screen x 0 */
        put_tile_rows(gb, 0x8000, 0x00, 0x00);   /* BG tile: shade 0 */
        put_tile_rows(gb, 0x8010, 0xFF, 0xFF);   /* window tile: shade 3 */
        fill_map(gb, 0x9800, 0x00);
        fill_map(gb, 0x9C00, 0x01);
        render_frame(gb);
        TEST_ASSERT(gb->ppu.framebuffer[0] == 3,
                    "WY=0 WX=7: the window covers (0,0); got %u",
                    (unsigned)gb->ppu.framebuffer[0]);
        TEST_ASSERT(gb->ppu.framebuffer[100 * GB_SCREEN_W + 50] == 3,
                    "WY=0 WX=7: the window covers the whole screen; got %u",
                    (unsigned)gb->ppu.framebuffer[100 * GB_SCREEN_W + 50]);
        t_free(gb);
    }

    /* --- case B: WX = 87 puts the window's left edge at screen x = 80 --- */
    {
        gb_t *gb = t_machine((const u8[]){ 0x00 }, 1);
        TEST_ASSERT(gb != NULL, "cart_load() failed");
        ppu_reset(gb);
        gb->ppu.lcdc = lcdc;
        gb->ppu.bgp  = 0xE4;
        gb->ppu.wy   = 0;
        gb->ppu.wx   = 87;
        put_tile_rows(gb, 0x8000, 0x00, 0x00);
        put_tile_rows(gb, 0x8010, 0xFF, 0xFF);
        fill_map(gb, 0x9800, 0x00);
        fill_map(gb, 0x9C00, 0x01);
        render_frame(gb);
        TEST_ASSERT(gb->ppu.framebuffer[79] == 0,
                    "WX=87: x=79 is still BG; got %u",
                    (unsigned)gb->ppu.framebuffer[79]);
        TEST_ASSERT(gb->ppu.framebuffer[80] == 3,
                    "WX=87: x=80 is WX-7, the window's first pixel; got %u",
                    (unsigned)gb->ppu.framebuffer[80]);
        t_free(gb);
    }

    /* --- case C: WY = 80 starts the window on line 80 --- */
    {
        gb_t *gb = t_machine((const u8[]){ 0x00 }, 1);
        TEST_ASSERT(gb != NULL, "cart_load() failed");
        ppu_reset(gb);
        gb->ppu.lcdc = lcdc;
        gb->ppu.bgp  = 0xE4;
        gb->ppu.wy   = 80;
        gb->ppu.wx   = 7;
        put_tile_rows(gb, 0x8000, 0x00, 0x00);
        put_tile_rows(gb, 0x8010, 0xFF, 0xFF);
        fill_map(gb, 0x9800, 0x00);
        fill_map(gb, 0x9C00, 0x01);
        render_frame(gb);
        TEST_ASSERT(gb->ppu.framebuffer[79 * GB_SCREEN_W] == 0,
                    "WY=80: line 79 is BG; got %u",
                    (unsigned)gb->ppu.framebuffer[79 * GB_SCREEN_W]);
        TEST_ASSERT(gb->ppu.framebuffer[80 * GB_SCREEN_W] == 3,
                    "WY=80: line 80 is the window's first line; got %u",
                    (unsigned)gb->ppu.framebuffer[80 * GB_SCREEN_W]);
        t_free(gb);
    }

    /* --- case D: the window's own line counter, not LY --- */
    {
        gb_t *gb = t_machine((const u8[]){ 0x00 }, 1);
        TEST_ASSERT(gb != NULL, "cart_load() failed");
        ppu_reset(gb);
        gb->ppu.lcdc = lcdc;
        gb->ppu.bgp  = 0xE4;
        gb->ppu.wy   = 8;
        gb->ppu.wx   = 7;
        put_tile_rows(gb, 0x8000, 0x00, 0x00);   /* BG: shade 0 */
        put_tile_rows(gb, 0x8010, 0xFF, 0xFF);   /* tile 1: shade 3 */
        put_tile_rows(gb, 0x8020, 0x00, 0x00);   /* tile 2: shade 0 */
        fill_map(gb, 0x9800, 0x00);
        fill_map(gb, 0x9C00, 0x02);              /* window rows 1..31 -> tile 2 */
        for (int i = 0; i < 32; i++) bus_write(gb, (u16)(0x9C00 + i), 0x01);
        render_frame(gb);

        TEST_ASSERT(gb->ppu.framebuffer[7 * GB_SCREEN_W] == 0,
                    "above WY the BG must show; got %u",
                    (unsigned)gb->ppu.framebuffer[7 * GB_SCREEN_W]);
        TEST_ASSERT(gb->ppu.framebuffer[8 * GB_SCREEN_W] == 3,
                    "the window's FIRST line uses its own row 0 (tile 1, shade 3). "
                    "A shade of 0 here means the row came from LY instead of the "
                    "window line counter; got %u",
                    (unsigned)gb->ppu.framebuffer[8 * GB_SCREEN_W]);
        TEST_ASSERT(gb->ppu.framebuffer[15 * GB_SCREEN_W] == 3,
                    "the first 8 window lines all use window row 0; got %u",
                    (unsigned)gb->ppu.framebuffer[15 * GB_SCREEN_W]);
        TEST_ASSERT(gb->ppu.framebuffer[16 * GB_SCREEN_W] == 0,
                    "the 9th window line uses window row 1 (tile 2, shade 0); got %u",
                    (unsigned)gb->ppu.framebuffer[16 * GB_SCREEN_W]);
        t_free(gb);
    }
}

/* ------------------------------------------------------------------------- */
/* L23 - STAT and LYC                                                        */
/* ------------------------------------------------------------------------- */

TEST(m07_stat_modes_timing)
{
    /*
     * Within a visible line: mode 2 (OAM scan) for dots 0-79, mode 3 (drawing)
     * for dots 80-251, mode 0 (HBlank) for the rest of the 456. STAT's low two
     * bits mirror the mode. The mode-3 length here is the 172-dot convention
     * from L04; L30 refines it.
     */
    gb_t *gb = t_machine((const u8[]){ 0x00 }, 1);
    TEST_ASSERT(gb != NULL, "cart_load() failed");

    ppu_reset(gb);
    gb->ppu.lcdc = 0x91;

    ppu_tick(gb, 1);
    TEST_EQ(gb->ppu.ly, 0);
    TEST_EQ(gb->ppu.mode, 2);
    TEST_ASSERT((gb->ppu.stat & 0x03) == gb->ppu.mode,
                "STAT bits 0-1 must report the live mode: mode=%u stat&3=%u",
                (unsigned)gb->ppu.mode, (unsigned)(gb->ppu.stat & 0x03));

    ppu_tick(gb, 79);                     /* dot 80 */
    TEST_EQ(gb->ppu.mode, 3);
    TEST_ASSERT((gb->ppu.stat & 0x03) == 3, "STAT must show mode 3 during drawing");

    ppu_tick(gb, 172);                    /* dot 252 */
    TEST_EQ(gb->ppu.mode, 0);
    TEST_ASSERT((gb->ppu.stat & 0x03) == 0, "STAT must show mode 0 during HBlank");

    ppu_tick(gb, 204);                    /* dot 456 -> next line */
    TEST_EQ(gb->ppu.ly, 1);
    TEST_EQ(gb->ppu.mode, 2);

    /* VBlank is mode 1 for all of lines 144..153. */
    ppu_tick(gb, (u32)(142 * GB_CYCLES_PER_SCANLINE));    /* LY = 143 */
    TEST_EQ(gb->ppu.ly, 143);
    ppu_tick(gb, 1);                                      /* LY = 144 */
    TEST_EQ(gb->ppu.ly, 144);
    TEST_EQ(gb->ppu.mode, 1);
    TEST_ASSERT((gb->ppu.stat & 0x03) == 1, "STAT must show mode 1 in VBlank");

    t_free(gb);
}

TEST(m07_lyc_coincidence)
{
    /*
     * LYC (FF45) is compared against LY every line. The comparison is exposed
     * as STAT bit 2, and when STAT bit 6 is set the coincidence requests the
     * LCD STAT interrupt.
     */
    gb_t *gb = t_machine((const u8[]){ 0x00 }, 1);
    TEST_ASSERT(gb != NULL, "cart_load() failed");

    ppu_reset(gb);
    gb->ppu.lcdc = 0x91;
    gb->ppu.lyc  = 1;                     /* LY is 0 right now, so no match */
    gb->ppu.stat = 0x40;                  /* LYC=LY interrupt enable          */
    GB_IF(gb) = 0x00;

    ppu_tick(gb, 1);
    TEST_EQ(gb->ppu.ly, 0);
    TEST_ASSERT((gb->ppu.stat & 0x04) == 0,
                "LY=0 with LYC=1 must clear the coincidence flag");
    TEST_ASSERT((GB_IF(gb) & GB_INT_STAT) == 0,
                "no coincidence means no STAT interrupt");

    ppu_tick(gb, 455);                    /* LY becomes 1 */
    TEST_EQ(gb->ppu.ly, 1);
    TEST_ASSERT((gb->ppu.stat & 0x04) != 0,
                "LY=1 with LYC=1 must set the coincidence flag (STAT bit 2)");
    TEST_ASSERT((GB_IF(gb) & GB_INT_STAT) != 0,
                "with STAT bit 6 set, the coincidence must request IF bit 1");

    /* The flag must also clear again when LY moves on. */
    ppu_tick(gb, GB_CYCLES_PER_SCANLINE);  /* LY becomes 2 */
    TEST_EQ(gb->ppu.ly, 2);
    TEST_ASSERT((gb->ppu.stat & 0x04) == 0,
                "LY=2 with LYC=1 must clear the coincidence flag again");

    t_free(gb);
}

/* ------------------------------------------------------------------------- */
/* L25 - raster effects                                                      */
/* ------------------------------------------------------------------------- */

TEST(m07_raster_scroll)
{
    /*
     * The classic raster effect: change SCX between scanlines. That only works if
     * the renderer samples the register PER LINE, during that line's mode 3,
     * rather than once per frame.
     *
     * The test sets SCX = y immediately before ticking line y, and the map is
     * built so that map columns 0-3 are a dark tile and columns 4+ are a bright
     * one. A per-line renderer therefore goes bright at the line where
     * (y >> 3) reaches 4, i.e. y = 32.
     *
     * The EXACT boundary line is not asserted: whether a renderer samples SCX at
     * dot 80 or at dot 252 of the same line is not observable and should not be
     * over-specified. What is asserted is that the boundary exists at all (a
     * whole-frame renderer has no boundary: it would use the last SCX, 143, for
     * every line and come out bright everywhere) and that it lands near y = 32.
     */
    gb_t *gb = t_machine((const u8[]){ 0x00 }, 1);
    TEST_ASSERT(gb != NULL, "cart_load() failed");

    ppu_reset(gb);
    gb->ppu.lcdc = 0x91;
    gb->ppu.bgp  = 0xE4;
    gb->ppu.scx  = 0;
    gb->ppu.scy  = 0;

    put_tile_rows(gb, 0x8000, 0x00, 0x00);   /* tile 0: shade 0 */
    put_tile_rows(gb, 0x8010, 0xFF, 0xFF);   /* tile 1: shade 3 */
    for (int row = 0; row < 32; row++) {
        for (int col = 0; col < 32; col++) {
            bus_write(gb, (u16)(0x9800 + row * 32 + col),
                      (u8)(col >= 4 ? 0x01 : 0x00));
        }
    }

    for (int y = 0; y < GB_SCREEN_H; y++) {
        gb->ppu.scx = (u8)y;                 /* a different scroll for every line */
        ppu_tick(gb, GB_CYCLES_PER_SCANLINE);
    }

    TEST_EQ(gb->ppu.framebuffer[0], 0);
    TEST_ASSERT(gb->ppu.framebuffer[143 * GB_SCREEN_W] == 3,
                "SCX=143 on line 143 puts map column 17 at x=0 (tile 1, shade 3); "
                "got %u",
                (unsigned)gb->ppu.framebuffer[143 * GB_SCREEN_W]);

    int first_bright = -1;
    for (int y = 0; y < GB_SCREEN_H; y++) {
        if (gb->ppu.framebuffer[y * GB_SCREEN_W] == 3) { first_bright = y; break; }
    }
    TEST_ASSERT(first_bright >= 30 && first_bright <= 34,
                "with SCX = y per line the bright region must start at y = 32 "
                "(a whole-frame renderer would have no boundary at all). "
                "Got first bright line %d",
                first_bright);

    t_free(gb);
}
