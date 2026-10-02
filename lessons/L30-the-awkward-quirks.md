# L30 — The awkward quirks

**Time** — theory ~30 min | coding ~45 min | verify ~15 min
**You will end with** — the four behaviours that separate "runs the games" from "passes
the hardware suites": the TIMA reload delay, the DIV phase, the OAM DMA stall, and the
STAT edge rule. AHA: *these are not bugs to fix; they are the hardware being odd.*
**Tests that must go green** — `m09_timer_overflow_delay`, `m09_div_apu_edge`,
`m09_oam_dma_timing`, `m09_stat_blocking`
**Depends on** — L16 (the timer), L23 (STAT), L29 (access-level ticking). You will edit
`gb/src/timer.c`, `gb/src/bus.c`, `gb/src/ppu.c` and `gb/src/cpu.c`.

---

## 1. Read this — the whole theory for today

Four unrelated oddities. Each is small, each is tested, and each is the kind of thing that
makes an emulator "almost right".

### 1. TIMA does not reload immediately

On overflow, `TIMA` goes `0xFF -> 0x00`, and then for the next **4 T-cycles** it reads
`0x00`. Only at the end of that window does it take `TMA` and raise `IF` bit 2. A game that
reads `TIMA` right after an overflow sees `0x00`, not the reload value.

There is a further twist worth knowing but not required: writing `TIMA` during those 4
cycles **cancels the reload**, and `TMA` is not copied. Games that poll `TIMA` for a
precise timer can observe this. `m09_timer_overflow_delay` checks the delay only; mooneye's
timer suite checks the cancellation.

Implementation: `timer_t` already has `reload_delay` and `overflow_pending` fields for
this. On overflow set `overflow_pending = true; reload_delay = 4;` and count the delay down
as you tick; when it reaches zero, do `tima = tma` and raise the interrupt. While the
window is open, leave `tima` at `0x00`.

### 2. DIV is not a register, and writing it resets the phase

`DIV` (`FF04`) is the **high byte of the free-running divider**, and `TIMA` is clocked by a
bit of that same counter (L16). So writing `DIV` does two things at once: it makes the
visible value 0, and it **restarts the clock phase** that drives `TIMA`.

The consequence is testable without any obscure rule: after a `DIV` write, the next `TIMA`
tick is a **full period** away, never the remainder of the period that was interrupted.
`m09_div_apu_edge` ticks half a period, writes `DIV`, and requires no tick until a whole
period has passed.

The related glitches — a `DIV` write or a `TAC` write *injecting* a spurious tick when the
selected divider bit is high — are real but under-documented, and references disagree on
the exact conditions. This test deliberately does not assert them; mooneye's `timer/`
tests are the oracle. If you want them, implement them last, and let those tests tell you
whether you got it right.

### 3. OAM DMA stalls the CPU

Writing `FF46` starts a 160-byte copy from `value << 8` to `FE00-FE9F`. Two facts matter:

* It takes **160 M-cycles = 640 T-cycles**.
* During that time the CPU cannot reach the bus: it is effectively stalled, and only HRAM
  (`FF80-FFFE`) is usable — which is why real games' DMA wait routines are copied into HRAM
  before they run.

`m09_oam_dma_timing` asserts the stall: it steps the machine and requires `PC` not to move
on for at least 640 T-cycles after the `FF46` write. An implementation that copies the 160
bytes instantly passes the *contents* check and fails the timing check — deliberately.

Implementation: `bus_t` already has `dma_*` fields. On the `FF46` write, arm the transfer
(source high byte, index 0, 640 cycles left). In `bus_tick`, advance it 4 T-cycles at a
time, copying the matching byte. While `dma_active`, `cpu_step` returns 4 without fetching
(that is the stall), and `bus_read`/`bus_write` outside HRAM are blocked (reads `0xFF`,
writes dropped).

### 4. VRAM and OAM are unreachable while the PPU draws

The PPU owns VRAM and OAM during mode 3 (drawing), and OAM additionally during mode 2
(OAM scan). Blocked reads return `0xFF`; blocked writes are dropped. Games rely on this —
`oam_bug` is a whole test ROM about the consequences.

The window table:

| Region | Accessible in modes |
| --- | --- |
| VRAM `8000-9FFF` | 0, 1, 2 (not 3) |
| OAM `FE00-FE9F` | 0, 1 (not 2, 3) |

Implementation: `bus_read`/`bus_write` ask the PPU "are you in a mode that allows this
range?" and return `0xFF` or drop the write. With `LCDC.7 = 0` everything is accessible.

### 5. STAT is an edge, and it blocks

L23 already implemented the rising-edge rule. The DMG addition is **STAT blocking**: a
source that is already high does not retrigger, and there is a family of spurious/missed
interrupts around writing `STAT` and around the mode changes. `m09_stat_blocking` pins the
core rule — a source that stays high must not keep requesting the interrupt, but a new
rising edge after it goes low must. Anything beyond that is L31's long tail.

> **What matters for the code you are about to write**
>
> * **TIMA reload delay**: `overflow_pending` + `reload_delay = 4`. While pending, `TIMA`
>   reads `0x00`; when the delay expires, `tima = tma` and `IF` bit 2. The test ticks 16,
>   checks `TIMA == 0x00`, then ticks 4 and checks `0x42`.
> * **DIV**: writing `FF04` zeroes `div_counter` **and** resets the remembered bit used for
>   the falling-edge detection. Without the second half the next tick can arrive early.
> * **OAM DMA**: arm on the `FF46` write, progress 4 T per `bus_tick`, and while active
>   `cpu_step` returns 4 **without fetching** — that is the stall the test measures. Do not
>   implement the stall as "the CPU runs but its accesses are ignored"; PC would advance and
>   the test fails.
> * **Access windows**: VRAM is blocked only in mode 3; OAM is blocked in modes 2 and 3.
>   Return `0xFF` for blocked reads and drop blocked writes.
> * **STAT**: keep `stat_line` and the rising-edge comparison from L23. Clearing `IF`
>   between ticks in a test does not change your logic; the source must have gone low and
>   high again to request again.
> * These four features are independent. Land them one at a time, running its test after
>   each, and re-run `-Test` between them — `m09_access_cycle_costs` is the canary for
>   breaking the tick accounting.

---

## 2. Your task — 45 min

1. **The TIMA reload delay** (15 min). In `timer.c`: on overflow set the pending flag and a
   4-cycle countdown; count it down in `timer_tick`; complete the reload at zero. Run
   `m09_timer_overflow_delay`.
2. **The DIV phase reset** (5 min). In the `FF04` write path, reset the divider **and** the
   remembered bit. Run `m09_div_apu_edge`.
3. **OAM DMA** (20 min). Arm on `FF46`, progress in `bus_tick`, stall in `cpu_step`, block
   non-HRAM access while active. Run `m09_oam_dma_timing`.
4. **The access windows** (10 min, no test yet) and **STAT blocking** (5 min). Add the mode
   checks to `bus_read`/`bus_write`; confirm the STAT rule with `m09_stat_blocking`.
5. **Full regression** (5 min). This lesson touched four components:
   ```
   .\gb\build.cmd -Test
   ```
   Only the four `m09_*` tests you were aiming at should change state.

---

## 3. Prove it — 15 min

```
.\gb\build.cmd -Test m09
```
```
   5 passed,    0 failed,    0 skipped
ALL GREEN
```
Then a real game, because the DMA stall is the one of these four that visibly breaks games
when it is wrong (torn sprites, corrupted OAM, or a game that hangs waiting for a transfer
that never finishes):

```
.\gb\build\gbemu.exe --rom <a game with sprites>.gb --frames 600 --dump-frame build\q.bmp
```

And the test ROM that exists specifically for the access windows:

```
.\gb\build\gbemu.exe --rom roms\blargg\oam_bug.gb --serial - --frames 4000
```

`oam_bug` is a suite of eight sub-tests about OAM corruption and access windows. Do not
expect all eight today — the OAM *corruption* bug (16-bit inc/dec of a value in
`FE00-FEFF` during the first ~20 cycles of a visible line) is L31's territory. Passing the
access-window tests is the goal here.

---

## 4. If it fails

| Symptom | Cause | Find it |
| --- | --- | --- |
| `m09_timer_overflow_delay` sees `0x42` immediately | you reload at the overflow instead of 4 T later | set the pending flag, complete at zero |
| `TIMA` reads `0xFF` in the window | you are still counting, or you wrote `TMA` into `tima` early | the window reads `0x00` |
| `m09_div_apu_edge` ticks early after the DIV write | you reset `div_counter` but not the remembered bit | both, together |
| `m09_oam_dma_timing` reports a huge elapsed time | the DMA never ends, so PC never moves | `guard` in the test catches it; check the countdown |
| `m09_oam_dma_timing` reports ~4 T | you copy instantly and do not stall | the stall is the contract |
| OAM contents are wrong | you copied from the wrong source page, or only part of it | 160 bytes from `value << 8` |
| Games hang in their DMA routine | your stall also blocks HRAM | HRAM must stay accessible |
| The screen corrupts during sprite-heavy moments | VRAM/OAM access windows are not enforced, so the PPU sees writes mid-draw | modes 0-2 for VRAM, 0-1 for OAM |
| `m09_stat_blocking` fires repeatedly | you request on the level instead of the edge | keep `stat_line` |
| `m09_access_cycle_costs` broke | the DMA stall or a mode check calls `bus_tick` twice | one tick per access |

---

## 5. Done when

- [ ] All five `m09_*` tests are green
- [ ] `-Test` shows no regression outside `m09`
- [ ] `oam_bug`'s access-window sub-tests pass (the corruption ones may still fail — L31)
- [ ] A real game with sprites runs without visual corruption over 600 frames
- [ ] You can explain, in one sentence each, why TIMA delays, why DIV affects TIMA, why DMA
      stalls the CPU, and why OAM is unreachable in mode 2
- [ ] Commit message like `feat: TIMA reload delay, DIV phase, OAM DMA stall, access windows (L30)`

---

## 6. Optional, only if you have time

* `docs/03-memory-and-cartridge.md` §3.6 and `docs/04-ppu-and-peripherals.md` §4.3 — the
  DMA and access-window sections, written for the same behaviours.
* Implement `mem_timing-2`'s internal-cycle refinements now if `mem_timing` already passes:
  the ordering within `LD (HL+),A` and the idle M-cycle after a 16-bit read. That is the
  difference between passing `mem_timing` and passing `mem_timing-2`.

Next: **[L31 — The halt bug and the long tail](L31-the-halt-bug-and-the-long-tail.md)**
