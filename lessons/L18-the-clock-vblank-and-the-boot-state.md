# L18 — The clock, VBlank, and the boot state

**Time** — theory ~20 min | coding ~45 min | verify ~25 min
**You will end with** — **the whole machine running on one clock**, a VBlank interrupt
that fires 59.7 times a second, the documented post-boot register state — and
blargg's `cpu_instrs` printing `Passed`. AHA #4: *my CPU is correct.*
**Tests that must go green** — `m06_boot_without_bootrom`, `m06_frame_is_70224`,
`m06_vblank_interrupt_fires`
**Gate** — blargg `cpu_instrs` prints `Passed`
**Depends on** — L04 (the PPU skeleton), L16 (the timer), L17 (all opcodes + serial).
You will edit `gb/src/bus.c` (`bus_tick`), `gb/src/ppu.c` and `gb/src/cpu.c`.

---

## 1. Read this — the whole theory for today

### The one clock, finally connected

Until now every test drove a component directly: `ppu_tick`, `timer_tick`,
`cpu_step`. Real hardware has one clock, and the machine's job is to advance every
component by the same amount of time:

```
gb_step(gb):
    cycles = cpu_step(gb);      /* one instruction, or one interrupt dispatch */
    bus_tick(gb, cycles);       /* everything else advances by the same time  */
    gb->total_ticks += cycles;
```

`gb_step()` is provided — it has been calling your `bus_tick()` since L01. Today you
finally implement it:

```c
void bus_tick(gb_t *gb, u32 tcycles)
{
    timer_tick (gb, tcycles);
    ppu_tick   (gb, tcycles);
    serial_tick(gb, tcycles);
    /* apu_tick(gb, tcycles);   L35 */
}
```

That is the whole function. Coarse-grained (one tick per instruction) rather than
per-access; L29 moves the clock inside `bus_read`/`bus_write` for timing accuracy, and
when it does, `cpu_step` returns only the residual cycles so nothing is counted twice.
Design note, not a TODO: keep the tick call here, in one place.

### The frame, and where the VBlank interrupt comes from

| Constant | Value |
| --- | --- |
| T-cycles per scanline | 456 |
| Visible lines | `LY` 0..143 |
| VBlank lines | `LY` 144..153 |
| Frame | 154 x 456 = **70224 T-cycles** |

L04 gave you the dot counter, `LY`, the mode sequence and `frame_ready`. Today the PPU
must also **raise the interrupt** when it enters mode 1:

```c
if (entering mode 1) {
    gb->ppu.frame_ready = true;                       /* consumed by gb_step */
    bus_request_interrupt(gb, GB_INT_VBLANK);         /* IF bit 0            */
}
```

`gb_step()` lifts `ppu.frame_ready` into `gb->frame_ready`, and the provided
`gb_run_frame()` loops until it is set. So the chain is:

```
ppu_tick -> mode 1 -> frame_ready + IF bit 0
gb_step  -> lifts frame_ready
gb_run_frame -> stops, host presents a frame
```

The VBlank interrupt is the single most important interrupt in the system: it is how
every game paces itself. A game's main loop is usually `HALT` until VBlank, do work,
repeat.

### The post-boot state

Skipping the boot ROM means the machine must look as if the boot ROM just handed control
to `0x0100`. Fill `gb_apply_post_boot_state()` (in `src/cpu.c`) from this table — the
values are verified against Pan Docs and mooneye's `boot_hwio-dmgABCmgb`:

| Register | Value | | I/O | Value |
| --- | --- | --- | --- | --- |
| `AF` | `0x01B0` | | `FF00` P1 | `0xCF` |
| `BC` | `0x0013` | | `FF02` SC | `0x7E` |
| `DE` | `0x00D8` | | `FF04` DIV | `0xAB` |
| `HL` | `0x014D` | | `FF07` TAC | `0xF8` |
| `SP` | `0xFFFE` | | `FF0F` IF | `0xE1` |
| `PC` | `0x0100` | | `FF40` LCDC | `0x91` |
| `IME` | `0` | | `FF41` STAT | `0x85` |
| `IE` | `0x0000` | | `FF47` BGP | `0xFC` |

`LCDC = 0x91` is why the screen is on, the background is enabled and tiles come from
`0x8000` without the game having to set anything. `F = 0xB0` (Z, H, C set) assumes a
non-zero header checksum — if the checksum byte is `0x00`, `H` and `C` are clear. Do not
assert `DIV` or `IF`: they depend on how long the boot ROM ran, which is why the test
checks only the nine values above.

### The gate: `cpu_instrs`

Everything you need is now in place: all base opcodes (L05-L17), interrupts (L15), the
timer (L16), serial (L17), the PPU clock and VBlank (today). blargg's `cpu_instrs`
exercises every instruction with boundary data and checks that no other register is
disturbed. It prints through the serial port.

```
.\gb\build\gbemu.exe --rom roms\blargg\cpu_instrs\individual\01-special.gb --serial - --frames 4000
```
```
01-special
Passed
```

Run the individual ROMs `01-special` through `11-op a,(hl)` (or the combined
`cpu_instrs.gb`) and treat any `Failed #N` as a specific bug: the number identifies the
sub-test. This is the first external proof that your CPU is right rather than
approximately right — and it is worth the eight lessons it took.

> **What matters for the code you are about to write**
>
> * `bus_tick` calls `timer_tick`, `ppu_tick`, `serial_tick` with the **same** T-cycle
>   count. Nothing sleeps, nothing polls, nothing ticks itself.
> * The PPU raises `GB_INT_VBLANK` **on entering mode 1**, once per frame — not every
>   tick while in mode 1, or the CPU will spend all its time in the handler.
> * `frame_ready` is set by `ppu_tick`; `gb_step` lifts it; `gb_run_frame` polls the
>   lifted flag. If you set `gb->frame_ready` directly from the PPU, `gb_step` will
>   clear it before the host sees it.
> * `gb_apply_post_boot_state` runs from `gb_reset`, which the harness calls when it
>   builds a test machine — so setting these values changes what the CPU tests start
>   from. Set them **exactly** as tabled; `AF = 0x01B0` and `LCDC = 0x91` are asserted.
> * Write the I/O half through `bus_write` (so the timer/PPU/serial owners see it),
>   not by poking fields, or `--no-boot-rom` and the tests will disagree.
> * `IE = 0` and `IME = 0` after boot: games enable interrupts themselves.
> * Before chasing the gate, confirm `-Test m0` is fully green. A red unit test is a
>   faster oracle than a ROM that prints `Failed #7`.

---

## 2. Your task — 45 min

1. **`bus_tick`** (10 min). Three calls, in `src/bus.c`. Then run
   `.\gb\build.cmd -Test m06` and note that `m06_frame_is_70224` may now pass.
2. **The VBlank interrupt** (10 min). In `ppu_tick`, raise `GB_INT_VBLANK` exactly once
   on the mode 1 transition. `m06_vblank_interrupt_fires` checks that line 143 does
   **not** raise it and line 144 does.
3. **The post-boot state** (20 min). Fill `gb_apply_post_boot_state()` from the table,
   CPU half directly and the I/O half via `bus_write`.
4. **Run the tests** (5 min):
   ```
   .\gb\build.cmd -Test m06
   ```
   Three green, two red (`m06_joypad_matrix` is L19; `m06_run_60_frames` needs a real
   ROM's worth of correctness but may already pass).

## 3. Prove it — 25 min (this is the gate)

```
.\gb\build.cmd -Test m06
```
```
[       OK ] m06_boot_without_bootrom
[  FAILED  ] m06_joypad_matrix         <- L19
[       OK ] m06_frame_is_70224
[       OK ] m06_vblank_interrupt_fires
...
   3 passed,    2 failed,    0 skipped
```
Now the gate. Get the ROMs first (`roms/README.md` has the sources), then:

```
.\gb\build\gbemu.exe --rom roms\blargg\cpu_instrs\individual\01-special.gb --serial - --frames 4000
```
Expect `01-special` then `Passed`. If the ROM hangs with no output, run each individual
ROM in order and find the first one that fails; the earlier the better, because
`02-interrupts` failing points at L15 while `11-op a,(hl)` failing points at L07.

Then the combined ROM, which is the true gate:
```
.\gb\build\gbemu.exe --rom roms\blargg\cpu_instrs\cpu_instrs.gb --serial - --frames 20000
```
`Passed` here is AHA #4. Record the exact output in `NOTES.md` — you will want to look
back at it when L29's timing work breaks something.

---

## 4. If it fails

| Symptom | Cause | Find it |
| --- | --- | --- |
| `m06_frame_is_70224` reports `LY = 145` at the frame boundary | your line loop increments `LY` one dot early or late | the boundary is *at* dot 456 |
| `frame_ready` is set but `gb_run_frame` never returns | you set `gb->frame_ready` instead of `ppu.frame_ready` | `gb_step` owns the lift |
| The VBlank interrupt fires hundreds of times a second | you raise `IF` every tick while in mode 1 | once, on the transition |
| `m06_boot_without_bootrom` fails with `LCDC = 0x00` | you poked `ppu.lcdc` and `bus_write` is expected, or you never called it | the test reads `gb->ppu.lcdc` |
| The machine starts at `PC = 0` | `gb_apply_post_boot_state` does not set `PC` | `PC = 0x0100` |
| `cpu_instrs` prints nothing at all | serial is broken, not the CPU | the `SC = 0x81` path from L17 |
| `cpu_instrs` prints `Failed #5` and stops | one sub-test failed; the test ROM stops the file there | run the individual ROMs to localise |
| The ROM runs but `A` is wrong after a specific instruction | a flag bug; the trace is your friend | `--trace build\trace.txt`, look at the last 20 lines |
| `cpu_instrs` never terminates | the timer is not ticking, or VBlank never fires | add a counter print to `bus_tick` for a scratch run |

---

## 5. Done when

- [ ] The three `m06_*` tests for this lesson are green
- [ ] **`cpu_instrs` prints `Passed`**, and the output is recorded in `NOTES.md`
- [ ] `-Test m0` is fully green from `m00` to `m06` except L19's joypad test
- [ ] You can name the four calls that make up one machine step, in order
- [ ] You can explain why the VBlank interrupt must be raised once per frame, not per tick
- [ ] Commit message like `feat(gb): one clock, VBlank interrupt, post-boot state (L18)`

---

## 6. Optional, only if you have time

* `docs/06-verification-and-tooling.md` §6.3 — the rest of the test-ROM catalogue, so you
  know what `instr_timing`, `mem_timing` and `halt_bug` will ask of you later.
* Run `cpu_instrs` with `--trace build\trace.txt --frames 1` and look at the first
  hundred lines. Seeing the ROM's real instruction stream is a good way to appreciate
  what you built, and the trace format is the tool you will use for every remaining bug.

Next: **[L19 — The joypad](L19-the-joypad.md)**
