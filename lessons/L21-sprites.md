# L21 — Sprites

**Time** — theory ~25 min | coding ~55 min | verify ~10 min
**You will end with** — 40 hardware objects drawn with correct DMG priority, flips and
palettes. AHA: *I understand why my first sprite implementation would have been upside
down and backwards in priority.*
**Tests that must go green** — `m07_sprite_priority_x`, `m07_sprite_10_per_line`
**Depends on** — L04 (BG rendering, palettes, the line clock). You will edit
`gb/src/ppu.c`.

---

## 1. Read this — the whole theory for today

### OAM: 40 entries, four bytes each

Objects live at `FE00-FE9F` (already routed to `gb->bus.oam` since L03):

| Byte | Meaning |
| --- | --- |
| +0 | Y position: **screen Y = stored Y - 16** |
| +1 | X position: **screen X = stored X - 8** |
| +2 | tile index (`0x8000 + tile * 16`), or for 8x16 the low bit is ignored |
| +3 | attributes (below) |

The two offsets are pure convention from how the hardware counts, and they are the
single most common sprite bug. A sprite whose stored `Y` is `16` appears on screen line
**0**; stored `X = 8` appears at screen column **0**.

An entry is hidden if any of these is true:

* stored `Y == 0` or stored `Y >= 160` (it is off the top or below the screen),
* stored `X == 0` (screen x = -8, entirely off the left edge).

### The attribute byte

| Bit | Meaning |
| --- | --- |
| 7 | BG priority: **1 = sprite goes *behind* non-zero BG pixels** |
| 6 | Y flip (vertical mirror) |
| 5 | X flip (horizontal mirror) |
| 4 | palette: 0 -> `OBP0`, 1 -> `OBP1` (CGB uses this as a VRAM bank selector) |

`OBP0`/`OBP1` work exactly like `BGP`: two bits per colour id, so
`shade = (palette >> (id * 2)) & 3`.

### DMG priority: two rules, and one of them is backwards from your instinct

1. **Only 10 objects per scanline are drawn.** The 10 are chosen by **ascending X**;
   ties are broken by **ascending OAM index**. Objects beyond the 10th are simply not
   drawn, even though they are on screen.
2. **Among the selected 10, the lowest X wins** where they overlap — and for equal X,
   the **lowest OAM index** wins.

That second rule is why the naive implementation is wrong:

```
naive:  for (i = 0; i < 40; i++) draw(sprite[i]);     /* i=39 ends up on top  */
correct: draw in DESCENDING priority order, so the highest priority lands last
```

The simplest correct approach is to build the list of candidates, sort it by
`(X, OAM index)`, keep the first 10, and then iterate that list **backwards** while
drawing. Then "last one drawn wins" produces the right answer.

### Per-pixel rules, in order

```
1. compute the pixel's colour id from the tile, honouring X/Y flips
2. id == 0  -> transparent: draw nothing, the BG shows through
3. else if (attr bit 7) and (the BG pixel's colour id != 0) -> the BG shows
4. else -> shade = (OBP0 or OBP1 >> (id * 2)) & 3
```

Note rule 3 carefully: the BG priority bit only suppresses the sprite where the BG
pixel is **non-zero**. A sprite with the priority bit set still draws over a BG pixel of
colour id 0.

### 8x16 mode

`LCDC` bit 2 selects `8x16` objects. Then the tile index's **bit 0 is ignored** and the
sprite is two tiles stacked: `tile & 0xFE` for the top 8 rows and `(tile & 0xFE) | 1` for
the bottom 8. A Y flip applies to the whole 16-pixel object, not to each tile
separately — which is the second-most-common sprite bug after the offsets.

> **What matters for the code you are about to write**
>
> * `screen_y = oam[i*4] - 16`, `screen_x = oam[i*4 + 1] - 8`. Hide when
>   `oam[i*4] == 0`, `oam[i*4] >= 160`, or `oam[i*4 + 1] == 0`.
> * A sprite is a candidate for line `LY` when
>   `screen_y <= LY < screen_y + (8 or 16)`.
> * Selection: sort candidates by `(screen_x, oam_index)` ascending and keep the first
>   **10**; then **draw them in reverse** so the winner is painted last.
> * `id == 0` means transparent, always — even when the BG priority bit is 0.
> * The BG-priority bit only wins against a BG pixel whose colour id is **non-zero**.
> * `m07_sprite_priority_x` checks three things in sequence: the lower-X sprite wins an
>   overlap, equal X falls to the lower OAM index, and a solid-transparent sprite over a
>   shade-3 background leaves the background visible.
> * `m07_sprite_10_per_line` puts 12 sprites at screen x = 0, 8, ... 88 and expects
>   x = 72 drawn but x = 80 and x = 88 dropped. It fails loudly if you drop by OAM index
>   instead of by X when the two disagree — so build the candidate list from X first.
> * Sprites are drawn **after** the BG and window for the same line, and they must not
>   disturb the BG where they are transparent. If your renderer writes the framebuffer
>   in one pass per layer, this falls out for free.

---

## 2. Your task — 55 min

Work in `gb/src/ppu.c`.

1. **The candidate list** (20 min). For the current line, walk OAM 0..39, apply the
   visibility rules, and collect `{screen_x, oam_index, ...}` for those intersecting the
   line. Sort by `(screen_x, oam_index)`, truncate to 10. Write this as a helper that
   fills a small fixed array — no allocation, no globals.
2. **Draw one sprite** (20 min). Given a sprite and a pixel column, fetch the two tile
   bytes (honouring flips and the 8x16 split), extract the colour id, apply
   transparency, the BG priority check and the palette.
3. **Wire it into the line renderer** (10 min). After the BG (and window) for the line,
   iterate the candidate list **in reverse** and draw. Verify by hand that a
   lower-X sprite ends up on top.
4. **Run the tests** (5 min):
   ```
   .\gb\build.cmd -Test m07
   ```
   `m07_sprite_priority_x` and `m07_sprite_10_per_line` green. The window and STAT tests
   stay red — L22/L23.

---

## 3. Prove it — 10 min

```
.\gb\build.cmd -Test m07
```
```
[       OK ] m07_sprite_10_per_line
[       OK ] m07_sprite_priority_x
[  FAILED  ] m07_window_position      <- L22
[  FAILED  ] m07_bg_scroll_wrap       <- L22
[  FAILED  ] m07_stat_modes_timing    <- L23
[  FAILED  ] m07_lyc_coincidence      <- L23
```
Then look at sprites with your eyes, because the unit tests cannot tell you that a
sprite is *aesthetically* offset. Write a scratch program that puts one solid sprite at a
known place over a patterned background and dump a frame:

```
.\gb\build\gbemu.exe --rom <a ROM with sprites>.gb --frames 300 --dump-frame build\sprites.bmp
```
For the fast version, extend `m07_visual_smoke` in a scratch copy to draw three sprites at
screen (12,8), (16,8) and (20,8) with different tiles and dump a BMP. Overlap them and
confirm the left-most wins in the image, not just in the assertion.

---

## 4. If it fails

| Symptom | Cause | Find it |
| --- | --- | --- |
| Everything is 16 pixels down and 8 right | you used stored Y/X directly | screen = stored - 16 / - 8 |
| Sprites appear but the last one in OAM always wins overlaps | you drew 0..39 in order | draw in reverse priority order |
| `m07_sprite_10_per_line`: the dropped sprites are the wrong ones | you dropped by OAM index instead of by X | selection is by ascending X first |
| A sprite is invisible | stored `Y` or `X` is 0, or the tile index points at empty VRAM | check the visibility rules; VRAM is zeroed by default |
| Sprites show as solid blocks | you drew colour id 0 instead of skipping it | id 0 is transparent |
| Sprites vanish over the background but should not | you applied the BG priority bit unconditionally | it only wins against non-zero BG pixels |
| Flipped sprites are sheared or duplicated | you flipped the tile index instead of the pixel column | flip `x` inside the tile, not the address |
| 8x16 objects show two unrelated tiles | you used `tile + 1` instead of `(tile & 0xFE) \| 1` | and a Y flip spans both halves |
| `m07_bg_tile_decode` regressed | you changed the BG path while adding sprites | sprites must be a second pass, not a branch inside the tile decoder |

---

## 5. Done when

- [ ] `m07_sprite_priority_x` and `m07_sprite_10_per_line` are green
- [ ] Your BMP shows overlapping sprites with the left-most on top
- [ ] The four L04 tests are still green (no BG regression)
- [ ] You can state the two priority rules and the two offsets without looking
- [ ] You can explain why the naive 0..39 loop draws the wrong winner
- [ ] Commit message like `feat(ppu): sprites with DMG priority, flips and palettes (L21)`

---

## 6. Optional, only if you have time

* `docs/04-ppu-and-peripherals.md` §4.2 — the OAM section, including the "hidden objects
  still consume a slot" detail that `m07_sprite_10_per_line` does not cover.
* Implement 8x16 mode now if you skipped it; `dmg-acid2` at L24 draws both sizes, so
  skipping it just moves the work.

Next: **[L22 — The window and scrolling](L22-the-window-and-scrolling.md)**
