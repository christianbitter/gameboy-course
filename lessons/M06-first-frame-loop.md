# M06 - The first frame

**Goal** - Make the machine run in real time: a 70224 T-cycle frame driven by a scanline counter, the VBlank interrupt, the joypad register matrix, the post-boot machine state, and enough of an output path to boot a real ROM headless for 600 frames.

**Estimated effort** - 8-12 h over 2 sessions. The scanline counter is an afternoon; the post-boot table and the headless ROM run are where the surprises are.

## Read first

- `docs/04-ppu-and-peripherals.md` sections 4.1 (the scanline state machine, `LCDC`/`STAT` bits), 4.4 (joypad), 4.6 (the frame loop). It owns all of the register semantics here.
- `docs/03-memory-and-cartridge.md` section 3.3 (boot ROM and why skipping it is the default, and what `gb_apply_post_boot_state` must produce).
- `reference/cheatsheet-flags-and-timing.md` - the verified post-boot register and I/O tables. `m06_boot_without_bootrom` asserts only a subset; the table is the reference for the rest.
- `gb/include/gb/gb.h` - `gb_apply_post_boot_state()`, `gb_run_frame()` (given), `gb->frame_ready`, `gb_write_bmp()`, `GB_SHADES`.
- `gb/include/gb/ppu.h` - `ppu_t` (`dot`, `mode`, `ly`, `framebuffer`, `frame_ready`) and the three signatures. `gb/include/gb/joypad.h` - `joypad_t` and `joypad_read()`.
- `docs/06-verification-and-tooling.md` section 6.6 for the trace workflow you will use on your first real ROM.
- Pan Docs: "LCD Registers", "Rendering" (the mode diagram), "Joypad Input", "Power-Up Sequence".

## Why this milestone exists

M05 gave you a machine that can react to interrupts but produces no time. M06 is the milestone where time comes from exactly one place - the PPU's dot counter - and every other component's clock hangs off it. The important consequence is architectural: after this milestone `--frames N` is a reproducible measure, the tracer's `ticks` column means something, and the instruction-stepped core is validated against a real ROM's expectations rather than against your own test programs. Two things about this milestone are deliberately unambitious. First, **no tile rendering**: `ppu_tick` produces `LY`, `STAT` modes and the VBlank interrupt, and leaves the framebuffer at a constant shade. A blank or garbage screen is an acceptable M06 outcome; "it boots, the serial output is sane, and the trace shows the ROM's init code running" is the goal. Second, **the post-boot table is not yours to guess**: a wrong `LCDC` or `PC` here makes a game fail before it executes a single byte of its own code, and that failure looks exactly like a CPU bug. Transcribe the table, then trust the test.

## Deliverable contract

`gb/src/cpu.c` - `gb_apply_post_boot_state(gb_t *gb)`, called by the given `gb_reset()` when no boot ROM is enabled:

| Register | Value | | Register | Value |
| --- | --- | --- | --- | --- |
| `AF` | `0x01B0` | | `SP` | `0xFFFE` |
| `BC` | `0x0013` | | `PC` | `0x0100` |
| `DE` | `0x00D8` | | `IME` | false |
| `HL` | `0x014D` | | `IE` (`bus.ie`) | `0x00` |
| `LCDC` | `0x91` | | everything else | from the reference table |

`m06_boot_without_bootrom` asserts exactly this list and nothing more: AF, BC, DE, HL, SP, PC, IME, LCDC, IE. It deliberately does **not** assert `DIV` or `IF`, because their post-boot values depend on how many cycles the boot ROM ran, and nobody should bet a test on that. Fill the rest of the I/O registers from `reference/cheatsheet-flags-and-timing.md` because games read them; leave `DIV`/`IF` at whatever your reset produces and write down why.

`gb/src/ppu.c`:

```c
void        ppu_tick  (gb_t *gb, u32 tcycles);
void        ppu_reset (gb_t *gb);            /* extend the given skeleton */
const u8   *ppu_framebuffer(const gb_t *gb); /* 160*144 shade indices 0..3 */
```

What `ppu_tick` must guarantee in M06:

| Quantity | Value |
| --- | --- |
| Scanline length | `456` T-cycles |
| Lines per frame | `154` (visible `0-143`, VBlank `144-153`) |
| Frame length | `70224` T-cycles |
| Mode 2 then 3 then 0 | lines `0-143`: `dot 0-79`, `80-251` (fixed 172 for now), rest |
| Mode 1 | lines `144-153`, the whole 456 T each |
| `frame_ready` | set **once** per frame, when mode 1 begins |
| `GB_INT_VBLANK` | requested **once** per frame, when mode 1 begins |

`ppu_tick` must tolerate any `tcycles` value: the given `gb_step()` and the tests call it with 4, but a test may call `ppu_tick(gb, 70224)` directly. Loop internally in fixed steps; do not assume the caller is fine-grained. No tile rendering, no window, no sprites - M07 owns all of that.

`gb/src/joypad.c`:

```c
void joypad_set  (gb_t *gb, int button, bool down);
void joypad_reset(gb_t *gb);
u8   joypad_read (gb_t *gb);     /* called from bus_read(FF00) */
```

`gb/src/bus.c` - add the live register wiring:

```
FF00 -> joypad_read(gb)          FF40 -> ppu.lcdc      FF41 -> STAT (writes: bits 3-6 only; reads: bits 2-0 live, bit 7 reads 1)
FF44 -> ppu.ly (writes ignored)  FF45 -> ppu.lyc
FF42/43/47/48/49/4A/4B -> stored in ppu_t (M07 uses them)
```

`gb_run_frame()` and the BMP/PPM writers are given. Read `gb_run_frame()` before you write `ppu_tick`: the handshake you must satisfy is the boolean it polls, and the loop only terminates if the clock keeps moving.

## Work order

1. `gb_apply_post_boot_state` from the reference table. Run `.\gb\build.cmd -Test m06_boot_without_bootrom`.
2. `ppu_tick` skeleton: `dot`, `ly`, `mode`, one line per 456 T, `ly` wrapping to 0 after 153. Assert the line count before adding the interrupt. Run `m06_frame_is_70224`.
3. Mode 1 for `ly >= 144`, `frame_ready` set when entering mode 1, and `bus_request_interrupt(gb, GB_INT_VBLANK)` at that same transition. Run `m06_frame_is_70224` again: consecutive frame boundaries must be exactly 70224 T apart, and the VBlank interrupt must fire once per frame, not once per line.
4. The provided `gb_run_frame()`: read it, then confirm that a `cpu_step()` that returns 0 while halted cannot deadlock it (see the traps). Run `m06_vblank_interrupt_fires` with `IE` bit 0 set.
5. `joypad_read`: the P14/P15 selection matrix, 1 = released, bits 6-7 read 1, unselected groups read all 1s. Then `joypad_set`. Run `m06_joypad_matrix`.
6. Wire `FF00` and `FF40-FF4B` in `bus_read`/`bus_write`, including `LY` as read-only and the `STAT` write mask. Re-run the joypad and VBlank tests through `bus_*` only.
7. `ppu_framebuffer()` returning the 160x144 shade-index array. In M06 it may be constant; the contract that matters is the pointer and the size.
8. `.\gb\build.cmd -Test m06` and `m06_run_60_frames`. Then the real test: `.\gb\build\gbemu.exe --rom roms\<a real game>.gb --frames 600 --headless --trace roms\trace600.txt --dump-frame roms\frame600.bmp`.
9. Read `trace600.txt` around the last 100 instructions. It should show the ROM's own code (not your test programs), no `UNIMPLEMENTED`, no illegal opcode, and a plausible repeating idle loop - games `HALT` in their main loop, so seeing `HALT` and a VBlank return is a good sign.
10. Full regression: `.\gb\build.cmd -Test m0`; commit.

## Acceptance tests

```
.\gb\build.cmd -Test m06
.\gb\build.cmd -Test m06_frame_is_70224
.\gb\build\gbemu.exe --rom roms\<real>.gb --frames 600 --headless --trace roms\trace600.txt
.\gb\build\gbemu.exe --rom roms\<real>.gb --frames 600 --dump-frame roms\frame600.bmp
```

Pass criterion: `m06_frame_is_70224`, `m06_joypad_matrix`, `m06_vblank_interrupt_fires`, `m06_boot_without_bootrom`, `m06_run_60_frames` all PASS; 600 frames of a real ROM complete without `UNIMPLEMENTED`, without an illegal opcode, and without a hang; `frame600.bmp` is a valid 160x144 BMP (a blank image is acceptable - it proves the dump path, not the renderer); the trace's `ticks` column advances by multiples of 4 and the ROM is in its own code.

## Common traps

- `cpu_step()` returning 0 T-cycles while halted. Symptom: `gb_run_frame()` spins forever, `--frames 1` never returns, host CPU pinned at 100%, and the trace stops at the `HALT`. Cause: "the CPU is halted, so nothing happened" implemented as a 0-cycle return, which freezes the PPU clock too - the VBlank that would wake the CPU can never arrive. Detect: `m06_run_60_frames`; also `--frames 1` on a ROM that halts early must return. A halted or stopped CPU must still consume 4 T-cycles per `cpu_step()` iteration.
- Frame length wrong by 456 or 4560. Symptom: `m06_frame_is_70224` fails at 69768 or 73224, and games run slow or fast by a small factor that never triggers a visible bug. Cause: 144 lines instead of 154, or VBlank modelled as one line instead of ten. Detect: count the lines in one frame in a test, not the cycles alone.
- The mode-1 transition mishandled. Symptom A: `GB_INT_VBLANK` fires ten times per frame (once per VBlank line) and a game's VBlank counter runs at 10x. Symptom B: the frame boundary arrives 4560 T-cycles late and the first frame is `153 * 456` long. Cause A: raising the flag inside the per-4-cycle stepper instead of on the mode transition. Cause B: setting `frame_ready` after line 153 instead of when `ly` becomes 144. Detect: one frame with `IE` bit 0 set must dispatch exactly once, at tick `144 * 456`.
- `ppu_tick` assuming a 4-cycle caller. Symptom: everything works in tests that tick 4 at a time, then a direct `ppu_tick(gb, 70224)` produces one line, or `--max-cycles` runs skip frames. Cause: `dot += tcycles;` with a single boundary check instead of a loop. Detect: `ppu_tick(gb, 456)` and `ppu_tick(gb, 70224)` must advance `ly` by 1 and 154 respectively.
- `LY` write-through or `STAT` bits overwritten. Symptom: a game's `STAT` interrupt enable survives exactly one write and then vanishes; `LY` reads back what a hostile program wrote. Cause: `FF41 -> ppu.io[0x41]` instead of a masked read/write; `FF44` not read-only. Detect: write `0xFF` to `STAT`, read it back, expect `0x78 | (mode bits)`; write `0x00` to `LY`, read the live line.
- Joypad selection bits swapped, or pressed = 1. Symptom: the game reads every button as pressed, or the d-pad and buttons appear crossed (`m06_joypad_matrix` fails on the P14/P15 rows). Cause: `P15` (bit 5) selects buttons and `P14` (bit 4) selects directions; a *low* bit means pressed; bits 6-7 read 1. Detect: the test's 4-row matrix - both selected, only P14 low, only P15 low, neither low.
- Post-boot values applied in `cpu_reset`. Symptom: games boot fine with `--no-boot-rom` and misbehave if a boot ROM is ever enabled; or your own `cpu_reset` tests start seeing `PC = 0x0100` and `LCDC = 0x91` they did not ask for. Cause: putting the post-boot table where the *component* reset belongs. Detect: `cpu_reset` must leave a zeroed register file; `gb_apply_post_boot_state` is the only place the table lives.
- Wrong `frame_ready` member. Symptom: `--frames N` hangs or returns instantly; `m06_frame_is_70224` passes while the CLI does nothing. Cause: `ppu_t` has a `frame_ready` and `struct gb_s` has one too; the given `gb_run_frame()` polls one of them. Detect: read `gb/src/gb.c`, set the member it tests, and keep the other consistent or delete its use.

## Hint ladder

### H1

- How many T-cycles is one scanline, how many scanlines are in a frame, and which of those scanlines produce no picture? What is the product?
- What exactly happens to `PC`, `LY`, `LCDC` and `IE` when you start with no boot ROM, and which function is allowed to write those values?
- If a game executes `HALT` in its main loop and no interrupt is enabled, what advances the clock? Trace `gb_run_frame` -> `gb_step` -> `cpu_step` -> `bus_tick` and find the path that must still return cycles.
- Which two places in `gb_t`/`ppu_t` are named `frame_ready`, and which one does the given code poll?

### H2

- Technique: one monotone `dot` inside the line and one `ly`; derive the mode from `(ly, dot)` on every step. Never keep mode transition deadlines - they drift and they make M09 harder.
- Technique: test the PPU without any CPU at all. `gb_reset`, then `ppu_tick(gb, 4)` in a loop, recording `(total_ticks, ly, mode)` transitions into an array and asserting the boundary table. A 154-line boundary dump is the fastest way to see an off-by-one.
- Technique: for the first real ROM, use `--trace` and `--serial -` together. Serial output tells you the ROM got far enough to print; the trace tells you where it stopped. `docs/06-verification-and-tooling.md` section 6.6 has the loop.
- Spec pointers: docs/04 sections 4.1, 4.4, 4.6; docs/03 section 3.3; `reference/cheatsheet-flags-and-timing.md` for the post-boot table.

### H3

- Frame arithmetic: `154 * 456 = 70224`; visible lines `0-143`, VBlank lines `144-153`; mode 2 is `dot 0-79`, mode 3 starts at `dot 80` (172 T in this milestone), mode 0 until `dot 456`; mode 1 is the whole line for `ly >= 144`.
- `frame_ready` and `GB_INT_VBLANK` both belong to the *transition* into line 144, i.e. `ly == 144 && dot == 0`. `bus_request_interrupt(gb, GB_INT_VBLANK)` is given; use it instead of writing `FF0F` directly.
- Joypad read result: start from `0xC0`, set bit 4/5 from the stored `select`, and clear bit `n` for each pressed button in the selected group: `P14 = 0` -> bit 0 Right, 1 Left, 2 Up, 3 Down; `P15 = 0` -> bit 0 A, 1 B, 2 Select, 3 Start. If neither is selected, bits 0-3 read all 1s.
- Post-boot: `AF = 0x01B0`, `BC = 0x0013`, `DE = 0x00D8`, `HL = 0x014D`, `SP = 0xFFFE`, `PC = 0x0100`, `IME = 0`, `LCDC = 0x91`, `IE = 0`. `LCDC = 0x91` means LCD on, BG on, BG tile data at `0x8000`, BG map at `0x9800`. Everything else comes from the reference table.
- BMP/PPM are given: `gb_write_bmp(path, ppu_framebuffer(gb))` writes a 24-bit BMP using `GB_SHADES[4][3]`, and takes framebuffer *indices* `0..3`, not RGB. If your dump is all one colour, that is the framebuffer, not the writer.

## Done when

- `.\gb\build.cmd -Test m06` reports 5/5 PASS and `m0` shows M01-M05 still green.
- One frame is exactly 70224 T-cycles, measured two ways: `m06_frame_is_70224` and by counting `ly` transitions in your own throwaway test.
- `GB_INT_VBLANK` is requested exactly once per frame, proven by counting dispatches over a single frame.
- `gb_apply_post_boot_state` sets the nine asserted values, and `cpu_reset` alone still produces a zeroed register file.
- 600 frames of a real ROM run headless without `UNIMPLEMENTED`, an illegal opcode, or a hang, and `trace600.txt` shows the ROM's own code in a repeating idle loop.
- `frame600.bmp` opens as a 160x144 image, blank or otherwise, and `NOTES.md` records which ROM you used and how far it got.

## Stretch

- Add `--frames` timing output: cycles, frames, and the implied frames-per-second (`total_ticks / 70224 / elapsed`). It is a one-line sanity check that your frame length is right, forever.
- Implement the mode-3 length penalty table (SCX modulo 8) now and dump the STAT mode boundaries; M07 and M09 both want it and it costs a dozen lines.
- Make the joypad interrupt edge-accurate: remember the last read value (`joypad_t.prev_read` exists for this) and raise `GB_INT_JOYPAD` on a high-to-low transition of any *selected* bit. Then write the test that presses a button with no group selected and asserts no interrupt.

## Commit

```
feat(ppu,joypad,cpu): 70224-cycle frame, VBlank interrupt, joypad matrix, post-boot state, headless 600 frames (M06 green)
```
