# L22 — The window and scrolling

**Time** — theory ~25 min | coding ~50 min | verify ~15 min
**You will end with** — `SCX`/`SCY` scrolling across a wrapping 256x256 map, and the
window: a second background layer with its own line counter. AHA: *the window is not
"the BG at an offset", and the difference is a counter.*
**Tests that must go green** — `m07_bg_scroll_wrap`, `m07_window_position`
**Depends on** — L04 (BG decode), L21 (the two-pass renderer). You will edit
`gb/src/ppu.c`.

---

## 1. Read this — the whole theory for today

### The background is a window onto a 256x256 map

The tile map is 32x32 **bytes** = 256x256 **pixels**. `SCX`/`SCY` say which part of that
256x256 space the top-left of the screen shows, and the mapping **wraps**:

```
sx = (x + SCX) & 0xFF          /* 0..255, wrapping */
sy = (y + SCY) & 0xFF
map_index = (sy >> 3) * 32 + (sx >> 3)
tile_pixel_x = sx & 7
tile_pixel_y = sy & 7
```

Two consequences:

* **The mask is what makes it wrap.** Without `& 0xFF` you index past the map and read
  whatever is in VRAM after it.
* **Crossing a tile boundary mid-line is normal.** With `SCX = 4`, screen x 0..3 comes
  from one map column and x 4..7 from the next. `m07_bg_scroll_wrap` has a case for
  exactly that, and another for `SCX = 255`, where screen x 0 comes from map column 31
  and x 1 wraps back to column 0.

Your L04 renderer probably computed `sx = x` and `sy = y` and `map_x = x >> 3`. Today
those three expressions grow the scroll terms.

### The window is a separate layer with a separate line counter

| Register | Meaning |
| --- | --- |
| `WY` (`FF4A`) | the **screen line** where the window's first row appears |
| `WX` (`FF4B`) | the window's left edge **plus 7**: screen x = `WX - 7` |
| `LCDC.5` | window enable |
| `LCDC.6` | which map the window uses: 0 -> `9800`, 1 -> `9C00` (independent of the BG's `LCDC.3`) |

The window is drawn where `y >= WY` **and** `x >= WX - 7`. Note `LCDC.4` (tile data
addressing) is shared between the BG and the window.

The trap is the row source. For the BG, the map row comes from `(y + SCY) >> 3`. For the
window it comes from its **own line counter**:

```
window_line  -> the number of lines the window has drawn SO FAR this frame
map_row      = (window_line >> 3) & 31
tile_pixel_y =  window_line & 7
```

`window_line` starts at 0 each frame, and **increments only on lines where the window
was actually drawn** — not on every line, and not while `y < WY`. If you use `LY` (or
`LY - WY`) as the row source, the first window line looks right and every subsequent one
is wrong, which is why the bug survives a casual glance.

`m07_window_position` case D is built to catch precisely this: the window starts at
`WY = 8`, the window map's row 0 is a bright tile and rows 1+ are dark. A correct
renderer shows bright on screen lines 8..15 and dark from line 16; an `LY`-based row
source shows dark at line 8, because `LY >> 3` is already 1 there.

### Draw order

```
for each visible line y:
    1. background   (SCX/SCY, LCDC.3 map, wrap)
    2. window       (if LCDC.5 and y >= WY, from x = WX-7 rightwards)
    3. sprites      (L21, in reverse priority order)
```

The window is drawn **over** the background where it is active; it is not a separate
viewport you switch to. Sprites come last and can be behind the BG only via their
attribute bit.

`WX < 7` means the window starts partly off the left edge: the first `7 - WX` pixel
columns are cut off. `m07_window_position` uses `WX = 7` (left edge at x 0) and
`WX = 87` (left edge at x 80), so handle those two exactly and treat the `WX < 7` case
as a documented unknown to check against `dmg-acid2` at L24.

> **What matters for the code you are about to write**
>
> * `sx = (x + SCX) & 0xFF` and `sy = (y + SCY) & 0xFF` — the mask is what wraps the
>   256x256 space. `map_index = (sy >> 3) * 32 + (sx >> 3)`; the tile's pixel column is
>   `sx & 7` and its pixel row is `sy & 7`.
> * Keep `window_line` in `gb->ppu` (`window_line` already exists in the struct) and
>   **reset it to 0 at the start of each frame**, i.e. when `LY` wraps to 0.
> * `window_line` increments **once per line where the window drew**, never on lines
>   above `WY` and never twice for one line.
> * The window's left edge is `WX - 7`; its tile column is `(x - (WX - 7)) >> 3` and its
>   pixel column is `(x - (WX - 7)) & 7`.
> * The window's map base comes from `LCDC.6` (`0x9C00` when set, `0x9800` when clear),
>   which is independent of the BG's `LCDC.3`. `m07_window_position` sets both maps
>   differently on purpose.
> * `m07_bg_scroll_wrap` is eleven cases in one test, each on a fresh machine, and its
>   failure message prints `SCX`, `SCY`, the pixel and the reason — so read the message
>   before guessing which expression is wrong.
> * Do not reset `window_line` when the window is *disabled* mid-line beyond the
>   documented rule "increments only on lines where it drew": a game that turns the
>   window on halfway down the screen depends on the counter being 0 at that moment.

---

## 2. Your task — 50 min

Work in `gb/src/ppu.c`.

1. **Scrolling** (20 min). Thread `SCX`/`SCY` through the BG path with the wrap masks.
   Run `m07_bg_scroll_wrap` — its eleven cases will tell you exactly which term is
   missing, because each case names its own reason.
2. **The window layer** (25 min). After the BG for a line, if `LCDC.5` and
   `y >= WY`, draw from `x = max(0, WX - 7)` to 159 using `LCDC.6`'s map and
   `window_line` as the row source. Then — and only then — increment `window_line`,
   because the counter advances once per drawn line.
3. **Reset the counter per frame** (5 min). `window_line = 0` when `LY` wraps to 0.
   Without this the window is correct on frame 1 and silently wrong forever after, which
   is a bug `dmg-acid2` will not forgive.
4. **Run the tests** (5 min):
   ```
   .\gb\build.cmd -Test m07
   ```
   `m07_bg_scroll_wrap` and `m07_window_position` green. Only the two STAT tests remain
   red — L23.

---

## 3. Prove it — 15 min

```
.\gb\build.cmd -Test m07
```
```
[       OK ] m07_bg_scroll_wrap
[       OK ] m07_window_position
[  FAILED  ] m07_lyc_coincidence      <- L23
[  FAILED  ] m07_stat_modes_timing    <- L23
...
   7 passed,    2 failed,    0 skipped
```
Then prove the *animation*, which no unit test can: a correct scroll is obvious in motion
and invisible in a still frame. Add a scratch test that renders two frames with
`SCX = 0` and `SCX = 1`, dumps both to `gb/build/scroll0.bmp` and `gb/build/scroll1.bmp`,
and compare them by eye — every pixel column should have moved one place left.

For a real ROM, `--dump-frame` twice with a bigger `--frames` value is enough:
```
.\gb\build\gbemu.exe --rom <a ROM with a scrolling background>.gb --frames 60  --dump-frame build\a.bmp
.\gb\build\gbemu.exe --rom <a ROM with a scrolling background>.gb --frames 90  --dump-frame build\b.bmp
```
If `a.bmp` and `b.bmp` are identical, the ROM is not scrolling (or your `SCX` is being
ignored) — and if the image forms a diagonal smear, your wrap mask is wrong.

---

## 4. If it fails

| Symptom | Cause | Find it |
| --- | --- | --- |
| The image shifts but the map leaks garbage at the edges | missing `& 0xFF` | the wrap mask on `sx`/`sy` |
| `SCX = 4` shows a clean 4-pixel boundary but the wrong tiles | you used `x >> 3` for the map column instead of `(x + SCX) >> 3` | both the map index and the pixel column come from `sx`/`sy` |
| Scrolling works horizontally but `SCY = 8` does nothing | `scy` is not threaded into the row computation | `sy` feeds both `map_index` and `tile_pixel_y` |
| The window appears but shows the BG's tiles | you used `LCDC.3`'s map for the window | the window map is `LCDC.6` |
| The window's first line is right, the rest repeat it | `window_line` never increments | increment it once per drawn line |
| The window's first line is wrong immediately | you used `LY` as the row source | the window's own counter |
| The window appears one line late | you increment `window_line` *before* drawing the line | draw, then increment |
| The window is right on frame 1 and wrong after | you never reset `window_line` at the frame boundary | reset when `LY` wraps to 0 |
| The window starts one pixel off | `WX` is the left edge **plus 7** | `x - (WX - 7)` |
| Sprites vanished after adding the window | you replaced the BG+window pass instead of adding a layer | order: BG, window, sprites |

---

## 5. Done when

- [ ] `m07_bg_scroll_wrap` (all eleven cases) and `m07_window_position` (all four cases) are green
- [ ] Your two scratch frames show the background moving exactly one pixel per `SCX`
- [ ] The seven non-STAT PPU tests are all green, including the four from L04 and the two from L21
- [ ] You can write the `sx`/`sy` expressions and the window row source from memory
- [ ] You can explain why using `LY` for the window's row looks right for one line
- [ ] Commit message like `feat(ppu): SCX/SCY wrapping and the window layer (L22)`

---

## 6. Optional, only if you have time

* `docs/04-ppu-and-peripherals.md` §4.2 — the window section, which also notes the
  `WX < 7` behaviour worth checking against `dmg-acid2`.
* Render the window and the BG into two separate debug bitmaps (`--dump-bg`,
  `--dump-window`) behind a flag. When `dmg-acid2` fails at L24, being able to see the
  layers apart is the difference between a ten-minute fix and an evening.

Next: **[L23 — STAT and LYC](L23-stat-and-lyc.md)**
