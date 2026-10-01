# 04 — The PPU, input, and the frame

## 4.1 The scanline state machine

The PPU draws 154 scanlines of 456 T-cycles each: `154 * 456 = 70224` T-cycles
per frame, 59.7275 frames per second. Every game that does raster tricks is
watching the registers in this table change:

| Register | Addr | Role |
| --- | --- | --- |
| LCDC | `FF40` | master control, see below |
| STAT | `FF41` | mode + interrupt enables + LYC=LY flag |
| SCY / SCX | `FF42` / `FF43` | background scroll |
| LY | `FF44` | current scanline, read-only |
| LYC | `FF45` | scanline to compare against |
| BGP | `FF47` | background palette |
| OBP0 / OBP1 | `FF48` / `FF49` | sprite palettes |
| WY / WX | `FF4A` / `FF4B` | window position |

Mode progression inside one scanline (`dot` counts 0..455):

| Mode | STAT bits | When | Duration |
| --- | --- | --- | --- |
| 2 OAM scan | `10` | `dot 0-79` | 80 T-cycles (fixed) |
| 3 drawing | `11` | `dot 80-…` | 172-289 T-cycles (varies!) |
| 0 HBlank | `00` | after mode 3 until `dot 456` | the remainder |
| 1 VBlank | `01` | all of scanlines 144-153 | 456 T/line, 4560 total |

Use a `dot` counter and compute mode transitions from it, rather than a mode
timer. Mode 3's variable length is exactly why `STAT` mode/length tests are
fiddly; for M07 you may use a fixed 172 for mode 3, and fix it in M09.

`LCDC` bits:

| Bit | Meaning |
| --- | --- |
| 7 | LCD enable (0 = LCD off: LY reads 0, mode 0, no interrupts, VRAM freely accessible) |
| 6 | window tile map: 0 = `9800`, 1 = `9C00` |
| 5 | window enable |
| 4 | BG/window tile data: 0 = `8800` (signed indices, base `9000`), 1 = `8000` (unsigned) |
| 3 | BG tile map: 0 = `9800`, 1 = `9C00` |
| 2 | OBJ size: 0 = 8x8, 1 = 8x16 |
| 1 | OBJ enable |
| 0 | BG and window enable *and* BG priority (on DMG, 0 disables both) |

`STAT` bits:

| Bit | Meaning |
| --- | --- |
| 6 | LYC=LY interrupt enable |
| 5 | mode 2 interrupt enable |
| 4 | mode 1 interrupt enable |
| 3 | mode 0 interrupt enable |
| 2 | LYC=LY coincidence flag (read-only) |
| 1-0 | mode (read-only) |

Writing `LY` is ignored. Writing `STAT` only stores bits 3-6. The `STAT`
interrupt is requested on a **rising edge of the OR of the enabled sources**
(DMG also has the "STAT blocking" quirk: a source that is already high does not
retrigger; M09 territory).

Set `frame_ready = true` when entering mode 1, so the host knows to present.

## 4.2 Rendering: implement in three passes

1. **Background only.** If the background is right, most games are legible.
   Verify against a static screen; ignore everything else.
2. **Window.** Needed for menus, dialogue boxes, status bars.
3. **Sprites.** Needed for anything interactive.

Resist doing all three at once — you will not be able to tell which one is wrong.

### Tiles

A tile is 8x8 pixels, 2 bits per pixel, stored as two bitplanes interleaved:

```
byte 2*n   : low  bitplane, bit 7 = leftmost pixel
byte 2*n+1 : high bitplane, bit 7 = leftmost pixel
color_id   = ((hi >> (7-x)) & 1) << 1 | ((lo >> (7-x)) & 1)
```

Tile data lives at `8000-97FF` (1536 tiles, but 384 per map region).
Tile maps are 32x32 bytes and only *reference* tiles:

* `9800-9BFF` map 0, `9C00-9FFF` map 1
* each byte is a tile index, and the map is 32x32 = 256x256 pixels
* with `LCDC.4 = 0` the index is a **signed 8-bit offset from `9000`**:
  index `0xFF` means tile at `8F00`, not `9F00`. This is the classic trap.

Rendering pixel `(x, y)` of the background:

```
sx = (x + SCX) & 0xFF        /* the 256x256 map wraps */
sy = (y + SCY) & 0xFF
map_index = (sy >> 3) * 32 + (sx >> 3)
tile      = tile_map[map_index]
color_id  = tile_pixel(tile, sx & 7, sy & 7)
shade     = (BGP >> (color_id * 2)) & 3
```

### The window

* The window is a *second* background layer that starts at `(WX-7, WY)`.
* `WX - 7` is the x coordinate; `WX = 7` means x = 0. `WX < 7` shifts the window
  partly off the left edge — check `dmg-acid2` before "fixing" it.
* The window is only drawn on lines with `LY >= WY` (and `LCDC.5`).
* **The window keeps its own line counter**, incremented only on lines where the
  window was actually drawn. Using `LY` as the row is the single most common
  window bug and it looks fine on the first window line.

### Sprites

OAM: 40 entries x 4 bytes at `FE00`:

| Byte | Meaning |
| --- | --- |
| 0 | Y position, **stored Y = screen Y + 16** |
| 1 | X position, **stored X = screen X + 8** |
| 2 | tile index (8x16: the low bit is ignored, second tile is `tile | 1`) |
| 3 | attributes: bit 7 BG priority (1 = behind non-zero BG), bit 6 Y flip, bit 5 X flip, bit 4 DMG palette select (`OBP0`/`OBP1`) |

DMG OBJ priority rules, in order:

1. Only **10 sprites per scanline** are drawn. Selection is by **ascending X**;
   ties are broken by ascending OAM index.
2. Among the selected 10, the **lowest OAM index wins** where they overlap.
3. A sprite pixel with `color_id == 0` is transparent.
4. If attribute bit 7 is set and the background pixel's `color_id != 0`, the
   background wins. (Sprites always win over `color_id == 0` background pixels,
   even with the priority bit set.)

`stored Y == 0` or `stored Y >= 160+16` hides a sprite; `stored X == 0` hides it
too (it is off-screen the other way).

A naive loop that draws sprites 0..39 in order leaves the *last* one on top —
the opposite of the hardware. Draw in descending priority or resolve per pixel.

### Palettes

`BGP`/`OBP0`/`OBP1`: 2 bits per color id.

```
shade = (palette >> (color_id * 2)) & 3
```

Default `BGP = 0xFC`, `OBP0 = OBP1 = 0xFF`. A usable DMG shade table:

| shade | RGB |
| --- | --- |
| 0 (lightest) | `0xE0, 0xF8, 0xD0` |
| 1 | `0x88, 0xC0, 0x70` |
| 2 | `0x34, 0x68, 0x56` |
| 3 (darkest) | `0x08, 0x18, 0x20` |

## 4.3 LCD on/off and restricted access

* `LCDC.7 = 0`: mode becomes 0, `LY` reads 0, no STAT/VBlank interrupts fire, and
  the panel shows a blank (white) screen. Games toggle this to instant-blank.
* While the PPU is in mode 3, VRAM and OAM reads return `0xFF`. Games do depend on
  this (`oam_bug`); implement it in M09.
* `LY = 153` behaves like the other VBlank lines, but the coincidence flag has
  quirks worth checking in the spec if `lyc` tests fail.

## 4.4 Joypad (`FF00`)

The register is a 2x4 matrix selected by two bits you write:

| Bit | Write | Meaning |
| --- | --- | --- |
| 4 | P14 | 0 = select direction keys |
| 5 | P15 | 0 = select button keys |

| Read bit | With P14=0 | With P15=0 |
| --- | --- | --- |
| 0 | Right | A |
| 1 | Left | B |
| 2 | Up | Select |
| 3 | Start | Down |

* `0` means pressed. Unselected groups read as all 1s. Bits 6-7 read 1.
* A **high-to-low transition** of any selected bit requests the joypad interrupt
  (`IF` bit 4). The transition is what matters, not the level.

Host integration: `joypad_set(gb, GB_BTN_A, true)` sets a bit in your own state,
and `bus_read(FF00)` combines it with the current selection. Sample the host
keyboard once per frame (or on every `FF00` read) — both work, as long as input
arrives at a plausible cycle.

## 4.5 Serial (`FF01` / `FF02`)

* `FF01 SB` = shift byte; `FF02 SC` = control.
* `SC` bit 7 = transfer start/finish, bit 1 = internal clock, bit 0 = shift clock.
* Internal clock: 8192 Hz, so one bit every 512 T-cycles and 4096 T-cycles per byte.
* **The shortcut that makes every test ROM work:** when software writes `SC = 0x81`,
  immediately output `SB`, set `SC = 0x01` and raise `IF` bit 3. Test ROMs poll
  `SC` bit 7 and only need the completion flag; they do not measure 4096 cycles.
* External clock with no link partner: the received byte is `0xFF`.

This is your `printf`. `gb_serial_byte()` is the hook; wire it to stdout or a file
and you can read blargg's verdicts.

## 4.6 The frame loop

```c
void gb_run_frame(gb_t *gb) {
    gb->frame_ready = false;
    while (!gb->frame_ready)
        gb_step(gb);          /* one instruction */
    /* present + poll host input here */
}
```

* A frame is 70224 T-cycles at 59.7275 Hz -> 16.74 ms. To run in real time, sleep
  the remainder; to run fast, do not.
* Host determinism rule: input changes only at frame boundaries (or at explicit
  cycle numbers), so a recorded session replays identically.
* Headless mode dumps a BMP or PPM after N frames; this is enough to verify the
  PPU and it costs you no dependency. A windowed front end is M11.

## 4.7 The five PPU bugs that cost the most time

1. Forgetting the `+16`/`+8` sprite offsets, or hiding sprites on `stored Y == 0`
   but not `stored X == 0`.
2. Using `LY` as the window's row instead of the window's own line counter.
3. Drawing sprites in ascending OAM order, so the wrong sprite wins overlaps.
4. Rendering the whole frame at VBlank: passes static-screen tests, breaks every
   mid-frame `SCX`/`LCDC`/palette write (status bars, wavy water, split screens).
5. `8x16` tile selection (`tile & 0xFE`, second tile `tile | 1`) and Y-flip within
   a 16-pixel-tall sprite.

## 4.8 What to verify with what

| Question | Oracle |
| --- | --- |
| Does the background decode correctly? | your own BMP dump of a static screen |
| Are the window and sprites correct? | `dmg-acid2` |
| Is the mode timing right? | `dmg-acid2` + `STAT` timing tests, then M09 |
| Is anything at all happening? | `--trace` + serial output |

## Sources

* **Pan Docs:** *Rendering* (the mode/dot timing table and the mode-3 penalty
  sources), *LCDC*, *STAT* (including the spurious STAT interrupt quirk),
  *Palettes*, *OAM* (the 10-per-line limit and DMG object priority),
  *Accessing VRAM and OAM*, *OAM DMA Transfer*, *Interrupt Sources*, and the
  joypad and serial register pages.
* **dmg-acid2** (MIT) is the oracle for everything in §4.2 and §4.3; its reference
  image tells you *which* rule is broken, so read its README before "fixing"
  anything. `mealybug-tearoom-tests` is the harder follow-up for mode-3 timing.
* Oracles: `dmg-acid2`, blargg's `oam_bug` and `mem_timing`, mooneye's
  `acceptance/ppu/` and `acceptance/oam_dma/`. Annotations:
  [../reference/external-references.md](../reference/external-references.md).

Next: **[05-audio.md](05-audio.md)** and **[06-verification-and-tooling.md](06-verification-and-tooling.md)**.
