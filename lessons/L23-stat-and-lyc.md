# L23 — STAT and LYC

**Time** — theory ~25 min | coding ~50 min | verify ~15 min
**You will end with** — the LCD status register: live mode bits, the `LYC=LY`
comparison, four interrupt sources and the rising-edge rule that drives them. That is the
last PPU piece, and it is the mechanism every raster effect depends on.
**Tests that must go green** — `m07_stat_modes_timing`, `m07_lyc_coincidence`
**Depends on** — L04 (the mode machine), L15 (`bus_request_interrupt`). You will edit
`gb/src/ppu.c` and the `FF41`/`FF45` cases in `gb/src/bus.c`.

---

## 1. Read this — the whole theory for today

### The STAT register

`FF41 STAT`, bit by bit:

| Bit | Name | R/W | Meaning |
| --- | --- | --- | --- |
| 6 | LYC interrupt | **W** | request an interrupt when `LY == LYC` |
| 5 | mode 2 interrupt | **W** | request when entering OAM scan (dot 0 of a line) |
| 4 | mode 1 interrupt | **W** | request when entering VBlank |
| 3 | mode 0 interrupt | **W** | request when entering HBlank |
| 2 | LYC=LY flag | **R** | 1 while `LY == LYC` |
| 1-0 | mode | **R** | 0 = HBlank, 1 = VBlank, 2 = OAM scan, 3 = drawing |

So **only bits 3-6 are writable**, and bits 0-2 are *derived* from the live PPU state
every time the mode or `LY` changes. A write must preserve the low bits:

```
stat = (stat & 0x07) | (value & 0x78)
```

Bits 0-2 must never be stored from a software write, or a game that writes `0x00` and
then polls the mode bits would see `0` forever.

### The LYC=LY comparison

`LYC` (`FF45`) is compared against `LY` on every line:

```
flag = (LY == LYC)                 /* exposed as STAT bit 2 */
```

The flag is *state*, not an event: it is set for the whole time `LY == LYC` and cleared
as soon as `LY` moves on. `m07_lyc_coincidence` checks all three transitions — clear at
`LY = 0` with `LYC = 1`, set at `LY = 1`, clear again at `LY = 2`.

### The four interrupt sources, and the rising-edge rule

A game catches HBlank to change scroll registers mid-frame, and catches
`LYC=LY` to split the screen at an exact line. Both arrive as the **same** interrupt:
`IF` bit 1, "LCD STAT". The four enables are OR-ed into one internal line:

```
stat_line = (enable_bit_3 && mode == 0)
         || (enable_bit_4 && mode == 1)
         || (enable_bit_5 && mode == 2)
         || (enable_bit_6 && (LY == LYC))
```

The interrupt is requested on a **rising edge** of that line:

```
if (stat_line && !gb->ppu.stat_line) bus_request_interrupt(gb, GB_INT_STAT);
gb->ppu.stat_line = stat_line;
```

`ppu_t` already has the `stat_line` field for exactly this. Edge semantics matter
because of the DMG's "STAT blocking" behaviour: a source that is *already* high does not
retrigger. Modelling the OR plus the edge gets you the common case right; the precise
spurious-interrupt quirk is L30.

Note the asymmetry with the VBlank interrupt: VBlank has its own bit (`IF` bit 0) and the
PPU raises it directly (L18); the STAT sources all share `IF` bit 1 and are gated by the
enable bits *and* the edge.

### LCD off

When `LCDC.7` is 0: mode is 0, `LY` reads 0, the coincidence flag is derived from
`LY == LYC` as usual, and no STAT interrupt is requested from the mode bits (there are no
modes). L04's `m07_lcd_off_blank` already pins the mode and `LY`; do not break it.

### What the tests pin

`m07_stat_modes_timing` walks one line dot by dot and asserts the mode at each boundary:

| After ticks | `dot` | `LY` | mode |
| --- | --- | --- | --- |
| reset | 0 | 0 | 2 |
| +1 | 1 | 0 | 2 |
| +79 | 80 | 0 | 3 |
| +172 | 252 | 0 | 0 |
| +204 | 456 -> 0 | 1 | 2 |
| +142 lines | 0 | 143 | 2 |
| +1 | 0 | 144 | 1 (VBlank) |

It also asserts that `stat & 0x03` equals `mode` on every one of those boundaries, so the
register and the internal state cannot drift apart.

> **What matters for the code you are about to write**
>
> * Updating the mode means writing **both** `gb->ppu.mode` and the low two bits of
>   `gb->ppu.stat`, every time. Do it in one helper so they cannot diverge.
> * `bus_write(FF41, v)` must apply `stat = (stat & 0x07) | (v & 0x78)`. The low three
>   bits come from the PPU, never from software.
> * Update the `LY == LYC` flag whenever `LY` changes, and expose it as STAT bit 2.
> * Compute `stat_line` as the OR of the enabled sources and request
>   `GB_INT_STAT` only on a **rising edge** (`stat_line && !gb->ppu.stat_line`), then
>   store it back into `gb->ppu.stat_line`.
> * Read the enables from the *stored* bits 3-6, i.e. after masking, so a test that
>   assigns `gb->ppu.stat = 0x40` directly still works.
> * `m07_lyc_coincidence` ticks 1 dot, then 455, then 456 — so your `LY` transition must
>   land exactly at dot 456, not at 455 or 457. If it is off by one, `m07_stat_modes_timing`
>   fails on the `LY = 1` boundary at the same time.
> * The STAT interrupt does **not** replace the VBlank one: `LCDC.7`/mode 1 has both its
>   own `IF` bit 0 and, if bit 4 is enabled, a contribution to `IF` bit 1.

---

## 2. Your task — 50 min

Work in `gb/src/ppu.c` and the `FF41`/`FF45` cases in `gb/src/bus.c`.

1. **The mode writer** (10 min). One helper that sets `gb->ppu.mode` **and**
   `gb->ppu.stat`'s low two bits together, then calls the STAT-line update. Route every
   place you currently assign `mode` through it.
2. **`STAT` read/write** (10 min). `bus_write(FF41, v)` preserves bits 0-2 and stores
   `v & 0x78`; `bus_read(FF41)` returns `stat` with the live low bits and the LYC flag.
   `FF45` maps to `ppu.lyc`.
3. **The LYC flag** (10 min). Update it whenever `LY` changes; expose it as bit 2.
4. **The rising edge** (15 min). Compute `stat_line`, request `GB_INT_STAT` on a rising
   edge, store it back. Add the VBlank-enter path as a mode-1 source (bit 4) without
   removing the `IF` bit 0 raise from L18.
5. **Run the tests** (5 min):
   ```
   .\gb\build.cmd -Test m07
   ```
   All **nine** green — the PPU is done except for L30's timing refinements.

---

## 3. Prove it — 15 min

```
.\gb\build.cmd -Test m07
```
```
   9 passed,    0 failed,    0 skipped
ALL GREEN
```
Then prove the *interrupt* works end to end, because a unit test on `IF` is not a program
catching HBlank. Write a scratch program that:

```
- sets LYC = 80 and enables the LYC interrupt (STAT bit 6)
- installs a handler at 0x0048 (the STAT vector) that increments a counter in HRAM
- enables interrupts and then HALTs in a loop
```

Run it for a couple of frames and confirm the counter reaches 2 (once per frame, when
`LY` crosses 80). That is a split-screen effect's entire mechanism, and you have now
implemented the thing that makes it possible.

```
.\gb\build.cmd -Test
```
Regression: everything from `m00` to `m07` green; `m08`+ still red or skipped.

---

## 4. If it fails

| Symptom | Cause | Find it |
| --- | --- | --- |
| `stat & 0x03` is 0 while `mode` is 2 | you update one and not the other | one helper, both fields |
| Writing `STAT` loses the mode bits | you stored the whole byte | `(stat & 0x07) \| (v & 0x78)` |
| Reading `STAT` shows a stale mode | you store the mode in a separate variable | derive bits 0-1 from `mode` on read |
| The LYC flag never sets | you compare against a stale `LY`, or update the flag only once per frame | update on every `LY` change |
| `m07_lyc_coincidence` sets `IF` at `LY = 0` too | you raised the interrupt on the *level* | rising edge only |
| `IF` is requested but never again all frame | you never clear `stat_line` | store the new value every time |
| STAT interrupts fire constantly during mode 2 | you enabled all four sources regardless of bits 3-6 | gate each source on its enable |
| `m07_stat_modes_timing` fails on the `LY` boundary | off-by-one in the wrap | `LY` changes at dot 456 |
| `m07_lcd_off_blank` regressed | you mode-write while the LCD is off | LCD off holds mode 0 and `LY` 0 |

---

## 5. Done when

- [ ] All nine `m07_*` tests are green
- [ ] Your scratch LYC-handler program increments a counter exactly once per frame
- [ ] `-Test` shows `m00`-`m07` green
- [ ] You can write the STAT bit table and the `stat_line` expression from memory
- [ ] You can explain why the STAT interrupt is edge-triggered and VBlank's is not
- [ ] Commit message like `feat(ppu): STAT modes, LYC coincidence, the STAT interrupt (L23)`

---

## 6. Optional, only if you have time

* `docs/04-ppu-and-peripherals.md` §4.1 and §4.3 — the STAT section, including the
  spurious-interrupt quirk that L30 will implement and the modes when the LCD is off.
* Implement the mode 0 (HBlank) interrupt and test it by polling `IF` in a loop. HBlank
  interrupts are how games do per-scanline effects, so having one working before L25 is
  worth the twenty minutes.

Next: **[L24 — dmg-acid2](L24-dmg-acid2.md)**
