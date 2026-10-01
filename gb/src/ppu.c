/*
 * ppu.c - the LCD controller. YOURS TO IMPLEMENT.
 *
 * Milestones:
 *   M06  the scanline skeleton: dot counter 0..455, LY, modes 2/3/0, mode 1 on
 *        lines 144-153, frame_ready on entering mode 1, VBlank bit in IF.
 *        NO rendering yet.
 *   M07  rendering: palettes, background, window, sprites, STAT interrupt
 *        sources, LYC coincidence, LCD-off behaviour.
 *   M09  the awkward parts: variable mode-3 length, VRAM/OAM restrictions
 *        during mode 3, STAT blocking, LY=153 details.
 *
 * Timing facts (docs/04): 456 T-cycles per scanline, 154 scanlines, so
 * 70224 T-cycles per frame. Mode 2 = 80 T. Mode 3 = 172-289 T (use 172 in M06
 * and refine in M09). Mode 0 = whatever is left of the line. Mode 1 = all 456 T
 * of each of lines 144-153.
 *
 * Rendering strategy: one scanline at a time, in order BG -> window -> sprites.
 * The reason is the trap in docs/04 section 4.7: a whole-frame-at-VBlank
 * renderer passes dmg-acid2 but silently breaks every mid-frame register write,
 * which is how games draw status bars and split screens.
 */
#include "gb/gb.h"

void ppu_reset(gb_t *gb)
{
    memset(&gb->ppu, 0, sizeof gb->ppu);
    /* memset leaves every framebuffer pixel at shade 0, i.e. a blank screen,
     * which is the right power-on state. */
}

/* PROVIDED: the host needs a stable pointer to the picture. */
const u8 *ppu_framebuffer(const gb_t *gb)
{
    return gb->ppu.framebuffer;
}

void ppu_tick(gb_t *gb, u32 tcycles)
{
    (void)gb; (void)tcycles;
    GB_UNIMPLEMENTED("ppu_tick(%u T-cycles) - M06: dot/LY/mode; M07: rendering",
                     tcycles);
}
