# L24 — dmg-acid2

**Time** — theory ~15 min | coding ~25 min | verify ~50 min
**You will end with** — the reference face rendered **pixel-perfectly**. This is the PPU's
external gate, and the first time your emulator is judged by an oracle you did not write.
**Tests that must go green** — none. The gate is a Bitmap comparison against the
reference image, and that is deliberate: a static face exercises combinations of rules
that no unit test enumerates.
**Depends on** — L04 (BG decode), L21 (sprites), L22 (window, scrolling), L23 (STAT).
You will edit `gb/src/ppu.c`, and probably only one line of it.

---

## 1. Read this — the whole theory for today

### What the test is

`dmg-acid2` is a small, deterministic, MIT-licensed ROM that draws a **face**. It is not
a random picture: every feature of the face is a deliberate probe of one rendering rule,
and the reference image in its repository is what a correct DMG produces. Wrong output
is not "garbage" — it is a *characteristic distortion* you can read.

That is why this gate exists on top of the nine unit tests. A unit test can check one
rule in isolation; acid2 checks that all the rules compose correctly in one frame, in the
order the hardware applies them.

### How to run it

```
.\gb\build.cmd
.\gb\build\gbemu.exe --rom roms\dmg-acid2.gb --frames 60 --dump-frame build\acid2.bmp
```

Sixty frames is plenty: the ROM draws a static image, so anything past the first frame is
identical. Then compare `build\acid2.bmp` with the reference PNG from the dmg-acid2
repository. Two ways:

```
python tools\compare_bmp.py build\acid2.bmp build\reference.bmp
```
which reports how many pixels differ and where the differences are concentrated, or just
open both images side by side.

Get the reference image from the repository listed in
`reference/external-references.md`; it is the `reference/` folder in the acid2 repo. Save
it as `build\reference.bmp` (convert the PNG with any tool, or use `ffmpeg -i ref.png
ref.bmp`).

### Reading the face

| Region of the face | Rule it exercises | Your lesson |
| --- | --- | --- |
| overall geometry, blank areas | BG tile decode, tile data addressing mode | L04 |
| the whole face shifted or repeated | `SCX`/`SCY` wrap, map selection | L22 |
| the hair / top area | the **window**: `WY`, and the window's own line counter | L22 |
| the eyes | sprites over BG, colour 0 transparency | L21 |
| the nose | **BG priority bit** (a sprite that must go *behind* non-zero BG) | L21 |
| the mouth | 8x16 objects, X and Y flips | L21 |
| the lower word / bottom band | the **10 sprites per scanline** limit and sprite-vs-sprite priority | L21 |
| palettes / grey levels | `BGP`, `OBP0`, `OBP1` extraction | L04 |

The acid2 repository's own README carries the authoritative feature-by-feature table and
shows what each broken rule looks like. Read it there rather than trusting this summary —
and then come back, because the method below is what makes it fast.

### The method (this is the actual lesson)

**Isolate by disabling a layer.** You have three render passes. If the face is wrong, do
not read code. Temporarily skip one pass and see what changes:

1. skip **sprites** entirely → the BG and window should still be correct. If they are not,
   the bug is L04/L22, and sprites are innocent.
2. skip the **window** → if the top of the face changes correctly and the rest does not,
   the bug is in the window's row source or its line counter.
3. skip the **BG** → the sprites should still be drawn, over shade 0.

This costs three two-minute rebuilds and removes two thirds of the search space each
time. It is the same "find the first divergence" idea as the tracer, applied to pixels.

**One change at a time, and re-run the suite.** A PPU fix that breaks
`m07_sprite_priority_x` is not a fix. The nine unit tests are your regression net for
exactly this hour.

### The determinism check

Two runs must produce byte-identical frames:

```
.\gb\build\gbemu.exe --rom roms\dmg-acid2.gb --frames 60 --dump-frame build\a.bmp
.\gb\build\gbemu.exe --rom roms\dmg-acid2.gb --frames 60 --dump-frame build\b.bmp
python tools\compare_bmp.py build\a.bmp build\b.bmp
```
Zero differing pixels. If not, something non-deterministic leaked into your machine —
an uninitialised field, a host clock, or a read of a stale buffer. That is a serious bug
and worth chasing even when the picture looks right.

> **What matters for the code you are about to write**
>
> * You are probably about to change **one** line. The rule is: change one thing, rebuild,
>   dump, compare, and run `-Test m07` to confirm no unit test regressed.
> * Do not "fix" acid2 by special-casing what you see. Every symptom maps to a rule in
>   the table above; find the rule.
> * Use the layer-disable technique before reading any code.
> * The tolerance is **zero pixels**. "Close" means a rule is still wrong — usually
>   sprite priority, the window row source, or the BG priority bit, in that order.
> * `tools/compare_bmp.py` reports a bounding box, and the *location* of the differences
>   is the real information: a difference confined to one band of eight pixels is a tile
>   row problem; a difference across a whole region is a layer problem.
> * Keep the passing BMP. It becomes your golden frame, and any later PPU change (L30's
>   timing work especially) is diffed against it.

---

## 2. Your task — 25 min

1. **Get both files** (10 min). The ROM (32 KiB, from the acid2 repo releases) and the
   reference image (from the same repo). Convert the reference to BMP if needed:
   `ffmpeg -i reference.png build\reference.bmp`.
2. **Run and compare** (5 min):
   ```
   .\gb\build\gbemu.exe --rom roms\dmg-acid2.gb --frames 60 --dump-frame build\acid2.bmp
   python tools\compare_bmp.py build\acid2.bmp build\reference.bmp
   ```
3. **Confirm determinism** (5 min) with the two-run diff above. Fix any non-determinism
   before touching the renderer.
4. **Note the shape of the error** (5 min). Write down, in `NOTES.md`, which regions
   differ and your first hypothesis. You will be tempted to skip this; the written
   hypothesis is what stops you from changing four things at once.

---

## 3. Prove it — 50 min

This is the debugging hour, and it is the most valuable one in the phase. Work the layers:

1. **Disable sprites** (temporarily, in a scratch copy) → is the BG/window correct?
2. **Disable the window** → is the BG correct everywhere?
3. Re-enable both and compare the bounding box again.
4. Map what is left to a row of the table in §1, fix that rule, re-run `-Test m07`, and
   compare again.

Success:

```
python tools\compare_bmp.py build\acid2.bmp build\reference.bmp
```
```
0 differing pixels
```
And, before you move on:
```
.\gb\build.cmd -Test m07
```
```
  10 passed,    0 failed,    0 skipped
```
Copy the passing BMP to `build\golden-acid2.bmp` and record in `NOTES.md` that this is
the frame every later PPU change must reproduce exactly.

---

## 4. If it fails

| What you see | Rule to check | Lesson |
| --- | --- | --- |
| The whole image is shifted, or tiles repeat every 256 pixels | `SCX`/`SCY` wrap mask | L22 |
| The face is present but the top band is a copy of a lower band | the window's **own line counter** | L22 |
| Eyes or mouth are missing entirely | sprite visibility: stored `Y`/`X` rules, or the 10-per-line limit dropping them | L21 |
| Overlapping sprites show the wrong one on top | draw order: descending priority, lowest X then lowest OAM index | L21 |
| A sprite that should be behind the background is in front | the BG-priority attribute bit, and the "only against non-zero BG" rule | L21 |
| Sprites are solid blocks instead of shapes | colour 0 must be transparent | L21 |
| Sprites are 8 pixels too low and 8 too far right | the `-16`/`-8` offsets | L21 |
| One 16-pixel-tall sprite shows two unrelated tiles | 8x16 tile pairing `(tile & 0xFE)` / `(tile & 0xFE) \| 1` | L21 |
| Everything is one shade | `BGP` extraction, or the palette never applied | L04 |
| The image is right but differs by one pixel in a band | a tile-decoder edge case at a bitplane boundary | L04 |
| Two runs differ | non-determinism: uninitialised state | L01's rules |

---

## 5. Done when

- [ ] `compare_bmp.py` reports **0 differing pixels** against the reference
- [ ] Two independent runs produce byte-identical frames
- [ ] `-Test m07` is 10/10 green — no unit test was sacrificed to get here
- [ ] The passing frame is saved as a golden artifact and noted in `NOTES.md`
- [ ] You can name which rule each region of the face probes
- [ ] You used the layer-disable method at least once, and it saved you time
- [ ] Commit message like `feat(ppu): dmg-acid2 passes pixel-perfect (L24)`

---

## 6. Optional, only if you have time

* `mealybug-tearoom-tests` (linked in `reference/external-references.md`) is the harder
  version: it checks the *timing* of mode 3 and mid-frame register writes, which is L30's
  territory. Do not start it before `dmg-acid2` is exact.
* Record a short video of acid2 plus your golden frame in `NOTES.md`. Together they are a
  complete statement of "the PPU is correct for static scenes".

Next: **[L25 — Raster effects](L25-raster-effects.md)**
