# L04 — First pixels

**Time** — theory ~25 min | coding ~55 min | verify ~10 min
**You will end with** — a BMP on your disk containing a picture that **your code**
drew, from VRAM you filled in by hand, with no CPU involved. AHA #3.
**Tests that must go green** — `m07_lcd_off_blank`, `m07_bg_tile_decode`, and
`m07_visual_smoke` (which writes `gb/build/l04_frame.bmp`).
**Depends on** — L03 (the bus answers reads and writes to VRAM). You will edit
`gb/src/ppu.c`, plus `FF40-FF4B` routing in `gb/src/bus.c`.

Why this lesson is here and not after the CPU: the PPU only needs VRAM, three
registers and a clock. Doing it now means you spend the rest of the course with a
screen you can look at, and every CPU bug from L05 on becomes visible rather than
theoretical.

---

## 1. Read this — the whole theory for today

### The picture

The framebuffer is 160x144 bytes, one byte per pixel, each value **0..3 = a shade
index that has already been through the palette**:

```
framebuffer[y * 160 + x]      y = 0..143 top to bottom, x = 0..159 left to right
0 = lightest, 3 = darkest
```

`ppu_framebuffer(gb)` returns it, and `gb_write_bmp(path, fb)` (provided) turns it
into a file you can open. Shades to RGB are in `GB_SHADES`.

### A tile is 16 bytes: two interleaved bitplanes

An 8x8 tile is stored as 8 pairs of bytes. In each pair, the first byte is the
**low** bitplane and the second is the **high** bitplane. Bit 7 of each byte is the
**leftmost** pixel:

```
byte 2*row      = low  bitplane
byte 2*row + 1  = high bitplane
colour id of pixel x = ((hi >> (7 - x)) & 1) << 1 | ((lo >> (7 - x)) & 1)
```

Worked example, `lo = 0xAA` (`1010 1010`), `hi = 0xCC` (`1100 1100`):

| x | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| lo bit | 1 | 0 | 1 | 0 | 1 | 0 | 1 | 0 |
| hi bit | 1 | 1 | 0 | 0 | 1 | 1 | 0 | 0 |
| colour id | 3 | 2 | 1 | 0 | 3 | 2 | 1 | 0 |

Now everything below is derivable from that one table.

### The tile map chooses the tile

* `9800-9BFF` is map 0 and `9C00-9FFF` is map 1; `LCDC` bit 3 selects which one.
* Each map is 32x32 bytes — one byte per 8x8 tile — covering 256x256 pixels.
* The map byte is a tile **index**. Which address it points at depends on `LCDC`
  bit 4:

| `LCDC.4` | Index interpretation | Formula |
| --- | --- | --- |
| 1 | unsigned, base `0x8000` | `addr = 0x8000 + index * 16` |
| 0 | **signed**, base `0x9000` | `addr = 0x9000 + (int8_t)index * 16` |

The signed mode is the classic trap: index `0x01` is `0x9010`, and index `0xFF` is
`0x8F00` — *not* `0x9F00`.

Pixel `(x, y)` of the background with `SCX = SCY = 0`:

```
map_x = x >> 3;  map_y = y >> 3
index = VRAM[map_base + map_y * 32 + map_x]
id    = tile_colour(tile_addr(index), x & 7, y & 7)
shade = (BGP >> (id * 2)) & 3
```

`SCX`/`SCY` scrolling is L22. Today they are zero and the map is 256x256, so
`(x, y)` maps straight through.

### The palette

`FF47 BGP` packs four 2-bit shades, one per colour id:

```
bits 1-0 = shade for colour id 0
bits 3-2 = shade for colour id 1
bits 5-4 = shade for colour id 2
bits 7-6 = shade for colour id 3
```

Useful values: `0xE4` is the **identity** (id N -> shade N), `0x1B` is an inverted
4-level ramp, `0xFF` maps every id to shade 3.

### The scanline clock

| Constant | Value |
| --- | --- |
| T-cycles per scanline | 456 |
| Visible lines | LY 0..143 |
| VBlank lines | LY 144..153 |
| Frame | 154 * 456 = **70224 T-cycles** |

The mode sequence inside one visible line (`dot` counts 0..455):

| Mode | STAT bits | When | Duration |
| --- | --- | --- | --- |
| 2 OAM scan | `10` | dot 0-79 | 80 |
| 3 drawing | `11` | dot 80-251 | 172 today (varies on hardware — L30) |
| 0 HBlank | `00` | dot 252-455 | the rest of the line |

**Render one line at a time**, when that line is finished (end of mode 3, i.e. the
start of HBlank). Do not render the whole frame at VBlank: that passes static
screens and silently breaks every game that changes `SCX`/`LCDC`/palette mid-frame.

**LCD off** (`LCDC.7 == 0`) is a distinct state: no modes, no rendering,
`LY` reads 0, `mode` is 0, and the screen is blank.

> **What matters for the code you are about to write**
>
> * `(hi >> (7-x)) & 1) << 1 | ((lo >> (7-x)) & 1)` — in that order. Swapping the
>   bitplanes gives you plausible-looking garbage, so this is the first thing to
>   check when a tile looks wrong.
> * `shade = (BGP >> (id * 2)) & 3`, then store **the shade**, not the colour id.
>   `m07_bg_tile_decode` uses `BGP = 0xE4`, so id == shade and the expected pixels
>   are literally the colour ids — that is why the identity palette is in the test.
> * Signed tile addressing uses `(int8_t)index` with base `0x9000`. Half of
>   `m07_bg_tile_decode` exists to catch getting this wrong.
> * Gate rendering on `LCDC.7`. `m07_lcd_off_blank` deliberately fills VRAM with a
>   pattern that *would* be all shade 3, then turns the LCD off and expects 144*160
>   untouched pixels.
> * `bus_tick` stays a stub today; the tests call `ppu_tick` directly. Do not
>   touch the CPU or the timer.

---

## 2. Your task — 55 min

1. **Route the PPU registers in `gb/src/bus.c`** (15 min). Replace the `FF40-FF4B`
   fall-through case with real routing, using the ownership table in
   `gb/include/gb/bus.h`:
   `FF40 -> ppu.lcdc`, `FF41 -> ppu.stat` (only bits 3-6 writable),
   `FF42/FF43 -> ppu.scy/ppu.scx`, `FF44 -> ppu.ly` (reads only; **writes
   ignored**), `FF45 -> ppu.lyc`, `FF47/FF48/FF49 -> ppu.bgp/obp0/obp1`,
   `FF4A/FF4B -> ppu.wy/ppu.wx`.
   Getting `FF44` write-ignored right matters: games write `LY = 0` to reset a
   raster effect, and it does nothing.
2. **The line clock in `ppu_tick`** (20 min). Add `dot` and advance it by the
   requested T-cycles. Derive `LY`, `mode` and `dot` from the running counter:
   * `dot 0-79` -> mode 2, `dot 80-251` -> mode 3, `dot 252-455` -> mode 0;
   * at `dot == 456` -> `dot = 0`, `LY++`;
   * `LY >= 144` -> mode 1 until `LY` wraps past 153 back to 0;
   * when the LCD is off, hold `LY = 0`, `mode = 0`, `dot = 0`.
   Set `gb->ppu.frame_ready = true` when mode 1 begins. (That flag's test is L18,
   but nothing is hurt by having it early.)
3. **`render_line(y)`** (15 min): the formula from §1. Loop `x` 0..159, compute the
   colour id from the two bytes of the tile, apply `BGP`, and write
   `gb->ppu.framebuffer[y * 160 + x]`. Only call it for `y < 144`.
4. **Run the tests** (5 min):
   ```
   .\gb\build.cmd -Test m07
   ```
   Three should be green (`m07_lcd_off_blank`, `m07_bg_tile_decode`,
   `m07_visual_smoke`); six are `SKIPPED` stubs for L21-L23.
5. **Look at your picture** (2 min). `m07_visual_smoke` wrote
   `gb/build/l04_frame.bmp`: open it. You should see vertical bands from lightest
   to darkest, then fine stripes. That image is entirely your code.

---

## 3. Prove it — 10 min

```
.\gb\build.cmd -Test m07
```
```
[       OK ] m07_bg_tile_decode
[  SKIPPED ] m07_bg_scroll_wrap  (write me: ...)
[       OK ] m07_lcd_off_blank
...
[       OK ] m07_visual_smoke
```
Then open `gb/build/l04_frame.bmp` and confirm you can see five distinct bands and
a striped one. If the bands are in the wrong order, your `BGP` extraction is
reversed; if they are vertical but 8 pixels wide instead of solid, check the map
indexing.

---

## 4. If it fails

| Symptom | Cause | Find it |
| --- | --- | --- |
| Every pixel is the same shade | you stored the colour id instead of `(BGP >> id*2) & 3`, or `BGP` is 0 | print `id` and `shade` for the first 8 pixels in a scratch test |
| The image looks like the right pattern but scrambled inside each tile | low and high bitplanes swapped | the high plane is the **second** byte of each row |
| Only the first 8 pixels of each line are correct | you forgot to advance the map column for `x >= 8` | `map_x = x >> 3` |
| `m07_lcd_off_blank` fails with pixels that are all shade 3 | you render regardless of `LCDC.7` | gate the whole render path on the LCD enable bit |
| `m07_bg_tile_decode`'s second half fails only | signed addressing: you used `0x9000 + index*16` unsigned | cast to `int8_t` first |
| The bottom of the BMP is blank, or you see 144 lines of nothing | your dot counter resets per call instead of accumulating | `dot` and `LY` live in `gb->ppu` and persist across `ppu_tick` calls |
| `gb/build/l04_frame.bmp` was not created | you ran the binary from the wrong directory, or the test aborted earlier | run the tests from the `gameboy-course` folder |

---

## 5. Done when

- [ ] `m07_lcd_off_blank` and `m07_bg_tile_decode` are green, both halves each
- [ ] `gb/build/l04_frame.bmp` exists and shows the five bands plus stripes
- [ ] `.\gb\build.cmd -Test m01` is still 6/6 (you changed `bus.c`)
- [ ] You can write the tile pixel formula from memory, including which byte is which plane
- [ ] You can explain why rendering happens per line instead of at VBlank
- [ ] Commit message like `feat(ppu): scanline clock, BG tile decode, palettes (L04)`

---

## 6. Optional, only if you have time

* `docs/04-ppu-and-peripherals.md` §4.1-4.2 — the same machine with the mode-3
  length variation and the scroll formula you will need in L22.
* Change `BGP` to `0x1B` in a scratch copy of the test and confirm the image
  inverts. That is the fastest way to prove your palette extraction is right.

Next: **[L05 — The CPU wakes up](L05-the-cpu-wakes-up.md)**
