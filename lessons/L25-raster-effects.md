# L25 — Raster effects

**Time** — theory ~20 min | coding ~30 min | verify ~40 min
**You will end with** — proof that your renderer samples the PPU registers **per line**,
which is the property that makes status bars, split screens and wobble effects work.
AHA: *the reason L04 refused the whole-frame shortcut, demonstrated.*
**Tests that must go green** — `m07_raster_scroll`
**Depends on** — L22 (window/scrolling), L23 (STAT, for the HBlank interrupt). You will
edit `gb/src/ppu.c` only if the test exposes something.

---

## 1. Read this — the whole theory for today

### The effect, and why it needs a per-line renderer

A Game Boy has no layers, no sprites-as-objects and no compositor. It has one framebuffer
being filled 144 times a second, and games create effects by **changing registers between
scanlines**:

| Effect | How it is done |
| --- | --- |
| a status bar that does not scroll with the level | `WY` set to the bar's line, or `SCX` changed above/below it |
| a wavy or "heat haze" background | `SCX` (sometimes `SCY`) written every line |
| a split screen with two different scrolls | `SCX`/`SCY` written at a specific `LY` |
| a palette fade or flash | `BGP`/`OBP0`/`OBP1` written mid-frame |
| letterboxing / window pull-in | `LCDC` bit 5 toggled per line |

Every one of those works only if the renderer reads the register **for the line it is
currently drawing**. A renderer that composes the whole frame at VBlank samples each
register once — at whatever value it happened to hold — and produces a static mess.

That is why L04 chose "one scanline at a time" and warned about the shortcut. Today you
prove you got the property, not just the plumbing.

### How a game synchronises with the raster

The CPU cannot write a register "at line 80" by magic. It waits:

* **polling**: read `LY` in a loop until it equals the target line. Simple, common, and
  it burns the whole CPU.
* **the HBlank interrupt** (STAT mode 0): enable `STAT` bit 3 and the handler runs at
  the end of every line, giving it the HBlank window to make its writes. This is L23's
  STAT work paying off.
* **the LYC interrupt** (STAT bit 6): run once per frame, at a chosen `LY`.

A typical status-bar routine is "wait for `LY == WY`, then write `SCX`". A typical wobble
is "in the HBlank handler, write `SCX = wobble_table[LY]`".

### What the test pins

`m07_raster_scroll` sets `SCX = y` immediately before ticking line `y`, with a map whose
columns 0-3 are dark and 4+ are bright. Screen x 0 of line `y` then samples map column
`y >> 3`, so the image must go bright at the line where `y >> 3` reaches 4.

Two assertions, and the distinction between them is deliberate:

* `framebuffer[0]` is dark and `framebuffer[143 * 160]` is bright — a whole-frame
  renderer using the last `SCX` (143) would be bright on **every** line, including line
  0, and fail here.
* the first bright line is near 32 (accepted range 30-34). The *exact* line is not
  asserted, because whether a renderer samples `SCX` at dot 80 or at dot 252 of the same
  line is unobservable and should not be dictated by a test.

So this test checks *the property*, not an implementation detail — which is exactly what
you want a raster test to do.

### What is still missing, and where it lives

Today's work is about **which line** sees a register change. It says nothing about
**which dot**, and dot-level accuracy is where the real hardware gets pedantic:
a write to `SCX` at dot 200 of a line affects that line on hardware but is racy, and
mid-line writes to `LCDC`/`WY` have their own rules. That is L30's territory, together
with the mode-3 length variation and the VRAM/OAM access windows.

> **What matters for the code you are about to write**
>
> * The property under test is "each line reads `SCX`/`SCY`/`LCDC`/`WX`/`WY`/palettes
>   when it renders". If your `render_line()` takes the registers as arguments captured
>   once per frame, it will fail; read them from `gb->ppu` inside the per-line function.
> * Nothing needs to be *added* for this test if L22 was done correctly — `SCX` is already
>   read per line. If the test is red, look for a cached value, a `static`, or a scroll
>   computed once at the frame boundary.
> * `m07_raster_scroll` fails in a specific way for the whole-frame renderer: no boundary
>   at all, and line 0 bright instead of dark. That signature is worth recognising,
>   because the same mistake also breaks every game with a status bar.
> * Do not "improve" the test by asserting the exact boundary line. The range is
>   intentional: it accepts any sampling point inside the line, which is all the hardware
>   guarantees at this level of accuracy.
> * After this test is green, `-Test m07` is **11/11** and the PPU is feature-complete for
>   static *and* dynamic scenes.

---

## 2. Your task — 30 min

1. **Run the test first** (5 min), before changing anything:
   ```
   .\gb\build.cmd -Test m07_raster_scroll
   ```
   If it is green already (likely, if L22 was done properly), the next step is to
   *understand why* rather than to code: find the line in `render_line()` that reads
   `scx` and confirm there is no cached copy.
2. **If it is red, find the cache** (20 min). Typical culprits, in order:
   * `scx`/`scy` captured into locals once per `ppu_tick` call and reused across lines,
   * a `static` holding the map base or the palette,
   * the frame rendered in a single pass at the `LY` wrap.
3. **Run the whole PPU suite** (5 min):
   ```
   .\gb\build.cmd -Test m07
   ```
   11/11 green.

---

## 3. Prove it — 40 min

```
.\gb\build.cmd -Test m07_raster_scroll
```
```
ALL GREEN
```
Then see it in a real game, because a test proves the property and a game proves the
feature. Pick a ROM with a **status bar** — a fixed band at the top or bottom that does
not move with the play area — and dump two frames that differ in scroll:

```
.\gb\build\gbemu.exe --rom <a game with a status bar>.gb --frames 300 --dump-frame build\hud1.bmp
.\gb\build\gbemu.exe --rom <a game with a status bar>.gb --frames 360 --dump-frame build\hud2.bmp
python tools\compare_bmp.py build\hud1.bmp build\hud2.bmp
```

What you want to see: the differences confined to the play area, with the status band
identical in both frames. If the whole screen scrolls together, the game's raster writes
are being ignored — and the bug is the same one `m07_raster_scroll` checks, just visible
in a way you cannot argue with.

Two more things worth doing with the hour:

* **Re-run `dmg-acid2`.** A raster fix must not break the static case:
  `python tools\compare_bmp.py build\acid2.bmp build\reference.bmp` must still say 0.
* **Add an HBlank-driven effect to a scratch test**: enable the mode 0 STAT interrupt
  (L23), have the handler write `SCX`, and assert the frame shows several different
  scrolls. That is the mechanism a real game uses, and building it once in a test makes
  L30's timing work much easier to debug later.

---

## 4. If it fails

| Symptom | Cause | Find it |
| --- | --- | --- |
| Line 0 is bright and there is no boundary | the whole frame is rendered once, with `SCX` sampled at the end | move the read inside the per-line path |
| The boundary is at the wrong line by more than a couple of lines | `SCX` is read once per *tick* rather than once per *line*, or `LY` is off | the boundary should track `y >> 3` |
| The test passes but a real game's status bar scrolls with the level | the game writes `SCX` during HBlank and your renderer has already drawn the line | you sample too late in the line; that is L30's dot accuracy |
| A game's wobble is diagonal rather than per-line | you apply `SCX` to the tile row as well as the column | `SCX` shifts columns, `SCY` shifts rows |
| The effect works for one frame then freezes | a `static` cached the first frame's registers | no `static` in the render path |
| `dmg-acid2` regressed after the fix | you moved the read but changed the wrap mask | re-run the acid2 diff |

---

## 5. Done when

- [ ] `m07_raster_scroll` is green and `-Test m07` is 11/11
- [ ] A real game with a status bar shows the band static across two scroll positions
- [ ] `dmg-acid2` is still 0 differing pixels
- [ ] You can name the three synchronisation methods a game uses to hit a scanline
- [ ] You can explain, in one sentence, why the whole-frame-at-VBlank renderer cannot
      produce raster effects
- [ ] Commit message like `feat(ppu): per-line register sampling verified with a raster test (L25)`

---

## 6. Optional, only if you have time

* `docs/04-ppu-and-peripherals.md` §4.1 and §4.7 — the mode table and the five classic
  PPU bugs, of which "rendering the whole frame at VBlank" is one you have now
  deliberately avoided.
* Try `mealybug-tearoom-tests` now. It will fail, and every failure is a dot-level timing
  question — which tells you exactly what L30 has to fix. Reading its output is a good way
  to see why L30 exists rather than being told.

Next: **[L26 — MBC1: banking](L26-mbc1-banking.md)**
