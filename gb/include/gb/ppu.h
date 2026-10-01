/*
 * ppu.h - the LCD controller: modes, scanlines, tiles, sprites, palettes.
 *
 * The STRUCT is provided. The BEHAVIOUR is yours (src/ppu.c).
 */
#ifndef GB_PPU_H
#define GB_PPU_H

#include "gb/common.h"

typedef struct gb_s gb_t;

typedef struct {
    /* Registers, as software sees them. */
    u8 lcdc;    /* FF40 */
    u8 stat;    /* FF41, bits 3-6 writable, bits 0-2 read-only */
    u8 scy;     /* FF42 */
    u8 scx;     /* FF43 */
    u8 ly;      /* FF44, read-only (writes ignored) */
    u8 lyc;     /* FF45 */
    u8 bgp;     /* FF47 */
    u8 obp0;    /* FF48 */
    u8 obp1;    /* FF49 */
    u8 wy;      /* FF4A */
    u8 wx;      /* FF4B */

    /* Internal state. */
    u16 dot;            /* 0..455 within the current scanline            */
    u8  mode;           /* 0 HBlank, 1 VBlank, 2 OAM scan, 3 drawing     */
    u8  window_line;    /* increments only on lines where the window drew */
    bool stat_line;     /* previous value of the OR of enabled STAT sources,
                           used to request an interrupt on a rising edge   */

    /* The picture. Values 0..3 are shade indices, already passed through the
     * palette by the renderer; the host converts them to RGB. */
    u8   framebuffer[GB_FB_SIZE];
    /*
     * ppu_tick() sets THIS flag when mode 1 begins. gb_step() copies it into
     * gb->frame_ready and clears this one; gb_run_frame() polls gb->frame_ready.
     * So ppu_tick() writes ppu.frame_ready, and nothing else does.
     */
    bool frame_ready;
} ppu_t;

void        ppu_tick  (gb_t *gb, u32 tcycles);
void        ppu_reset (gb_t *gb);
const u8   *ppu_framebuffer(const gb_t *gb);

#endif /* GB_PPU_H */
