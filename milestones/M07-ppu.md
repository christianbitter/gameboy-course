# M07 - The PPU: palettes, tiles, window, sprites

**Goal** - Extend M06's scanline skeleton so each visible line renders background, window and sprites into `ppu.framebuffer` with correct palettes, priority rules and STAT/LYC semantics.

**Estimated effort** - 10-14 h over 3 sessions (BG tile decode ~3 h, window and sprites ~4 h, STAT/LYC and LCD-off ~2 h, dmg-acid2 debugging ~3 h).

## Read first

- `gb/src/ppu.c` - the M06 skeleton you are extending, including the milestone split comment at the top. Read it before you plan anything.
- `gb/include/gb/ppu.h` - the frozen struct and the three signatures. `ppu_reset` and `ppu_framebuffer` are already implemented; `ppu_tick` is the function you fill in.
- `gb/src/bus.c` and `gb/include/gb/bus.h` - the FF40-FF4B cases you extend, and the rule that the bus is the only code that touches a component register.
- `docs/04-ppu-and-peripherals.md`: 4.1 (scanline state machine), 4.2 and its Tiles/window/sprites/palettes subsections, 4.3 (LCD on/off and restricted access), 4.7 (the trap that costs the most time), 4.8 (what to verify).
- `gb/tests/harness.h` and `gb/tests/t_m01_cart.c` for the fixture style: `TEST`, `TEST_ASSERT`, `TEST_EQ`, `t_machine`, `t_build_rom`, `t_load_rom_buffer`.
- Pan Docs "Rendering", and the `dmg-acid2` README - read its priority list before you write sprite code, not after.
- `docs/06-verification-and-tooling.md` for the serial harness and the frame-dump workflow.

## Why this milestone exists

M06 gave you the PPU's timing skeleton - the 456-dot counter, LY, the mode sequence, `frame_ready` - and nothing to look at. Two of the three PPU functions in `ppu.h` are already written; only `ppu_tick` is empty. M07 turns it into an image. The tempting shortcut is to ignore the skeleton and render the whole frame in one pass when VBlank arrives: it is 30 lines shorter, it makes dmg-acid2 pass, and it destroys every raster effect, because mid-frame scroll writes, split-screen status bars, mode-0 raster interrupts and scanline colour tricks all silently stop working. Rendering one scanline at the time M06 already schedules costs about 60 extra lines and gives you the timing model M09 and M11 depend on. The PPU is your first component with a real state machine, so it is also the cheapest place to learn how to debug one.

## Deliverable contract

You extend `gb/src/ppu.c` and the FF40-FF4B cases in `gb/src/bus.c`. Nothing in `ppu.h` changes:

```c
/* frozen in ppu.h */
void ppu_tick(gb_t *gb, u32 tcycles);        /* YOURS: the whole milestone lives here */
void ppu_reset(gb_t *gb);                    /* provided: memset, i.e. a blank screen  */
const u8 *ppu_framebuffer(const gb_t *gb);   /* provided: returns gb->ppu.framebuffer  */
```

```c
/* ppu_t members. The first line is M06's; do not move or rename it. */
u16 dot;  u8 mode;  u8 ly;  bool frame_ready;      /* M06 skeleton */
u8  lcdc, stat, scy, scx, lyc, bgp, obp0, obp1, wy, wx;
u8  window_line;                  /* increments only on lines where the window drew */
bool stat_line;                   /* previous OR of enabled STAT sources (M07)      */
u8  framebuffer[GB_FB_SIZE];      /* 160*144, row-major, top-left first (M07)       */
```

Framebuffer contract: one byte per pixel holding the **post-palette shade** 0..3, with 0 = lightest. That is why `ppu_reset`'s memset is a blank screen. Priority decisions are made on the pre-palette 2-bit pixel index; only the winning pixel is mapped through BGP/OBP0/OBP1. Do not store indices in the framebuffer and do not store shades in the priority comparison.

Register decode you add or extend in `bus.c`:

| addr | written by the CPU | read returns |
| --- | --- | --- |
| FF40 LCDC | stored | stored |
| FF41 STAT | bits 3-6 only | bit 7 = 1, bits 3-6 stored, bits 2-0 live |
| FF42/FF43 SCY/SCX | stored | stored |
| FF44 LY | ignored | live `ly` |
| FF45 LYC | stored | stored |
| FF47 BGP, FF48/FF49 OBP0/OBP1 | stored | stored |
| FF4A/FF4B WY/WX | stored | stored |

Layout you need on paper before coding:

```
tile at VRAM address B (16 bytes, 2 bytes per row)
  row 0: lo = B+0,  hi = B+1   ...   row 7: lo = B+14, hi = B+15
  column i (0 = leftmost) index = ???   <- derive from the two bytes

OAM entry (4 bytes, 40 entries at 0xFE00..0xFE9F)
  +0 Y (screen y = Y-16)   +1 X (screen x = X-8)   +2 tile (8x16: bit 0 ignored)
  +3 attributes: bit7 BG-over-OBJ, bit6 Y-flip, bit5 X-flip, bit4 palette (0=OBP0,1=OBP1)

scanline, 456 dots
 0                    80         80+len         455
 |--- mode 2, OAM ----|-- mode 3 --|-- mode 0 ---|
 LY 0..143 visible; LY 144..153 mode 1 for all 456 dots
```

Mode durations are M06's: mode 2 = 80 dots, mode 3 = 172 dots in the skeleton, mode 0 takes the rest, mode 1 occupies whole lines. M09 refines the mode-3 length and the restricted-access rules. Do not fork a second counter or a second boundary table in M07.

## Work order

1. Register decode: extend the FF40-FF4B cases in `bus.c` for BGP/OBP0/OBP1/SCY/SCX/WY/WX and the STAT write mask, keeping M06's existing cases. Run `gbemu_tests m07_lcd_off_blank`; expect PASS with LCDC bit 7 clear.
2. Static BG line with SCX=SCY=0 from a fixture: `t_machine(NULL, 0)` plus `bus_write` into VRAM. Run `gbemu_tests m07_bg_tile_decode`; expect PASS for both tile-data addressing modes (LCDC bit 4 set and clear).
3. Scrolling: make SCX/SCY wrap across the 32x32 tile map for values 0..255 and check the tiles straddling the seam. Run `m07_bg_scroll_wrap`.
4. Window: enable it, iterate WY/WX across boundary values, and make `window_line` advance only on lines that drew window. Run `m07_window_position`.
5. Palettes: map the winning index through BGP/OBP0/OBP1 into shades, and implement the index-0 rules (BG colour 0, OBJ index 0 always transparent).
6. Sprites: 8x8 then 8x16, per-sprite flip, palette select, the 10-sprites-per-line rule, X-then-OAM-index priority. Run `m07_sprite_priority_x` and `m07_sprite_10_per_line`.
7. STAT and LYC: wire the live mode bits, the LYC=LY flag and the enabled STAT sources (with `stat_line` for the rising edge) onto M06's mode machine. Run `m07_stat_modes_timing` and `m07_lyc_coincidence`.
8. Frame boundary: M06 already raises `frame_ready`; verify the framebuffer is complete and stable when it does, then dump a frame of your own ROM with `--dump-frame`.
9. `dmg-acid2` until it renders the smiley and the "yes" face with no stray pixels.
10. Regression: run every earlier filter in the same binary before you commit.

The two rendering strategies, because you will be tempted:

| strategy | cost | acid2 | raster effects |
| --- | --- | --- | --- |
| render whole frame at VBlank | ~30 lines less | passes | broken, silently |
| render the line M06 already scheduled | ~60 lines more | passes | work |

Take the second. If you keep a whole-frame path for speed, gate it behind a flag that is off by default.

## Acceptance tests

```
gbemu_tests m07_                 # add tests/t_m07_ppu.c; expect "ALL GREEN" and exit 0
gbemu --rom dmg-acid2.gb --frames 30 --ppm acid2.ppm
gbemu --rom <your-rom>.gb --frames 300 --dump-frame title.bmp
```

Tests the student must write: `m07_bg_tile_decode`, `m07_bg_scroll_wrap`, `m07_window_position`, `m07_sprite_priority_x`, `m07_sprite_10_per_line`, `m07_stat_modes_timing`, `m07_lyc_coincidence`, `m07_lcd_off_blank`.

Pass criteria: `gbemu_tests m07_` prints `ALL GREEN` and exits 0; `acid2.ppm` shows the smiley with the bottom "yes" face correct and no mismatched pixels in the BG-priority and 10-sprite regions; `title.bmp` shows a recognisable title screen; two identical runs produce byte-identical framebuffers.

## Common traps

- Whole-frame-at-VBlank rendering bolted onto M06's skeleton. Symptom: acid2 passes, then a game's status bar jitters or a scanline demo tears. Cause: the framebuffer was filled from register state sampled once per frame instead of per line. Detect: write SCX during mode 0, tick into the next line, assert the two lines differ.
- Bitplane bit order. Symptom: tiles look mirrored or the two planes are swapped. Cause: treating bit 0 of a byte as the leftmost pixel or swapping lo/hi. Detect: fill one row with 0x80/0x00 and assert pixel 0.
- Priority decided on shades instead of indices. Symptom: sprites fail to hide behind BG, or the wrong pixel wins. Cause: comparing post-palette values in the BG/OBJ priority test; it must use the raw 2-bit index, and index 0 is the only transparent BG colour. Detect: the BG-priority region of acid2.
- Sprite selection compared against the wrong Y. Symptom: sprites vanish on busy lines or the wrong ten are kept. Cause: comparing raw OAM Y to LY instead of LY+16, or applying the limit after X-sorting. Detect: `m07_sprite_10_per_line`.
- Sprite draw order. Symptom: overlapping sprites show the wrong one. Cause: sorting by OAM index only; on DMG lower X wins, and only on equal X does the lower OAM index win. Detect: `m07_sprite_priority_x` with two sprites at the same X.
- `window_line` bookkeeping. Symptom: window content scrolls, or the window starts one line off. Cause: incrementing the counter on lines where the window is disabled or WY > LY, or advancing it per pixel. Detect: `m07_window_position` with WY at a line boundary.
- WX origin. Symptom: the window is shifted 7 pixels or misbehaves for WX < 7. Cause: forgetting WX-7, the range check, or the small-WX special case. Detect: tests at WX=7, WX=0 and WX=166.
- LCDC bit 7 turned off mid-frame. Symptom: a freeze with a stale image, or LY stuck. Cause: LY/mode/framebuffer not reset and a `frame_ready` still delivered. Detect: `m07_lcd_off_blank`.

## Hint ladder

### H1

- M06's skeleton gives you `dot`, `mode` and LY. Where exactly does a visible line begin in `ppu_tick`, and what must be true about the framebuffer at that moment? Which mode has a variable length, and why must your rendering code not care?
- In the tile byte pair, which bit of which byte is the leftmost pixel? Answer from the spec, not from intuition.

### H2

- Technique: render into `framebuffer` at the first dot of each visible line, reading only the register state that exists at that dot, in the order docs/04 4.2 gives: BG, then window, then sprites. That is exactly what makes SCX/SCY writes between lines visible, and it needs no second counter.
- Fixture technique: the harness gives you `t_machine(NULL, 0)` for an empty machine and `bus_write` for VRAM/OAM. Do not require a real ROM for the eight `m07_` tests; raster bugs are 10x easier to find with a synthetic line than inside acid2.
- Spec pointers: docs/04 4.2 for the three passes and the priority rules, the `dmg-acid2` README for the priority matrix, Pan Docs "Rendering" for the mode diagram.

### H3

- Pixel decode: for row r of a tile at base B, `lo = B + 2*r`, `hi = lo + 1`, and column i (0 = leftmost) has index `((hi >> (7-i)) & 1) << 1 | ((lo >> (7-i)) & 1)`. BG shade = `(BGP >> (index*2)) & 3`; sprite index 0 is transparent, otherwise `(obp >> (index*2)) & 3` with OBP0/OBP1 chosen by attribute bit 4.
- Tile data base: LCDC bit 4 set -> 0x8000 with an unsigned tile index; clear -> 0x9000 with a signed index. BG map base: LCDC bit 3 set -> 0x9C00, clear -> 0x9800.
- A sprite is on the current line when `LY + 16` lies in `[oam_y, oam_y + height)`, with height 8 or 16 from LCDC bit 2.
- Window geometry: the window covers `x >= WX - 7`, requires WX <= 166 and WY <= LY, and its internal line counter starts at 0 on the first line it draws. Handle WX < 7 explicitly; that is where the off-by-seven bugs live.

## Done when

- `gbemu_tests m07_` reports 8/8 passing and prints `ALL GREEN`.
- dmg-acid2 renders the smiley and the "yes" face with no pixel mismatches, twice in a row.
- Your own ROM reaches a recognisable title screen via `--dump-frame`.
- Writing SCX during mode 0 of a line changes the next line but not the current one (raster writes work).
- Two identical runs produce byte-identical framebuffer dumps.

## Stretch

- Log the mode-3 length and HBlank entry dot you observe. M09 owns the penalty model (SCX modulo 8, window activation, sprite count); M07 only has to hand M09 a measurement to compare against.
- The DMG OAM scan bug and the mode-3 VRAM/OAM access restrictions are M09's territory: leave a flag and a TODO in the skeleton, do not implement them here.
- Build a VRAM/BG-map inspector that dumps the 256x256 BG map and the 16x24 tile sheet to BMP, so a glitch is traceable to a tile index in one glance.

## Commit

`git commit -am "ppu: per-line BG/window/sprite rendering, palettes, STAT/LYC, LCD off; dmg-acid2 passes"`
