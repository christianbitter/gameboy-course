# L20 — A real ROM runs

**Time** — theory ~15 min | coding ~30 min | verify ~45 min
**You will end with** — a ROM you did not write executing 600 frames on your machine, a
BMP on your disk, and a trace file. AHA #5, part one: *there is a real Game Boy program
running inside my code.*
**Tests that must go green** — `m06_run_60_frames` (so all five `m06_*`), plus the whole
suite from `m00` to `m06` green
**Depends on** — L18 (the clock, VBlank, the boot state). You will mostly *use* the CLI
that has been sitting in `gb/src/main.c` since L01.

---

## 1. Read this — the whole theory for today

### The loop you have been building towards

`gb/src/main.c` and `gb/src/gb.c` have been complete since L01. Today you finally run
them for real:

```
main:
    gb_create()                 one machine, all state
    gb_load_rom_file(path)      reads the file, calls cart_load, sets <rom>.sav
    gb_reset()                  wipe state, keep the cartridge, apply post-boot values
    for each frame:
        gb_run_frame(gb)        loops gb_step() until the PPU completes a frame
    gb_write_bmp(...)           if --dump-frame was given

gb_run_frame:
    frame_ready = false
    while (!frame_ready) gb_step()
    frame_count++

gb_step:
    cycles = cpu_step(gb)       one instruction or one interrupt dispatch
    bus_tick(gb, cycles)        timer + PPU + serial advance by the same time
```

Four functions you wrote, one you were given, and a real ROM's machine code on top.
There is nothing else.

### What `m06_run_60_frames` actually asserts

It loads a two-byte program — `0x18 0xFE`, which is `JR -2`, an infinite loop — and calls
`gb_run_frame` sixty times, then checks:

```
frame_count == 60
total_ticks >= 60 * 70224
total_ticks <  60 * 70224 + 60 * 32
```

The tolerance is the point: `gb_run_frame` can only notice a completed frame at an
*instruction boundary*, so a frame overshoots by up to one instruction. In this program
that is a 12 T-cycle `JR`. If your `total_ticks` is well above that, your PPU is
advancing faster than the CPU (a double tick), or the frame boundary is being hit at the
wrong dot.

This test is the machine-level integration check: CPU, bus, clock, PPU and the frame
boundary all have to agree for it to pass.

### Why a *32 KiB* ROM today, and not your favourite game

`cart_read` currently mirrors `0x4000-0x7FFF` to the same bank as `0x0000-0x3FFF` — correct
for a 32 KiB ROM-only cartridge, and wrong for everything bigger. Commercial cartridges
are MBC1/MBC3/MBC5 with 64 KiB to 8 MiB of ROM, so on a bigger ROM the CPU will fetch
from the wrong bank and either execute garbage or hang.

So today's target is a **32 KiB ROM**:

| ROM | Size | Why it is a good first target |
| --- | --- | --- |
| `dmg-acid2.gb` | 32 KiB | self-contained, deterministic, exercises the whole PPU (its visual verdict is L24) |
| any `mooneye` `acceptance/*` or `bits/*` ROM | 32 KiB | tiny programs that must terminate, so a hang is information |
| a small free homebrew / demo | varies | the most satisfying; check the header's ROM size code first |

Banking is L26. If you cannot wait and want a commercial game today, the optional step in
§6 lets you jump ahead — but the ladder puts it after the platform is proven, because a
banking bug and a CPU bug look identical from a black screen.

### What you should see, and what a failure looks like

For a 32 KiB ROM with graphics, `--frames 600 --dump-frame build\frame.bmp` should give
you a recognisable title screen or menu. The failure modes are all informative:

| What you see | Most likely cause |
| --- | --- |
| Nothing at all (all shade 0) | `LCDC.7` never set by the ROM's init, or `ppu_tick` not called from `bus_tick` |
| A black screen | `LCDC` set but the tile data or map is wrong |
| Garbage tiles | the tile decode (L04) or the signed/unsigned addressing mode |
| The ROM hangs with no output | an unimplemented opcode — your loud abort prints the opcode and PC |
| It runs forever but the frame never changes | the game is polling `LY`/VBlank: check that `bus_tick` advances the PPU |
| Wrong colours | `BGP` handling, or the shade table (L04) |

And for a ROM that prints, `--serial -` gives you text. That combination — a trace file, a
BMP and a serial log — is your complete evidence for every remaining bug in the course.

> **What matters for the code you are about to write**
>
> You write no new emulation code today — this lesson is about running and reading the
> machine you already built. But these are the things that decide whether a ROM runs:
>
> * `gb_reset()` applies your post-boot state (L18) and then executes **the ROM's own
>   code** at `0x0100`. Everything after that — palettes, `LCDC`, interrupts — is set up
>   by the game itself.
> * `--frames N` is a *hard* stop; without it (and without `--max-cycles`) `main.c` refuses
>   to run, which is deliberate: there is no window until L34.
> * `--trace FILE` writes one line per instruction. It is the single most useful flag in
>   the project; a 600-frame trace is large, so trace 1-2 frames when debugging.
> * `m06_run_60_frames`'s 32-cycle-per-frame tolerance is not slack you can spend
>   elsewhere: it exists because frames are observed at instruction boundaries.
> * If a ROM is larger than 32 KiB you will see the *first* symptom at whatever the game
>   does with banked data — usually a black screen or an immediate hang. Check the
>   header's ROM size code with `--info` before you debug anything else.

---

## 2. Your task — 30 min

1. **Run the test** (5 min):
   ```
   .\gb\build.cmd -Test m06
   ```
   All five `m06_*` green, which means `m00`-`m06` is now entirely green. If
   `m06_run_60_frames` fails, fix it before touching a ROM: it is a much smaller bug.
2. **Get a 32 KiB ROM** (5 min). `roms/README.md` has the sources; `dmg-acid2` is the
   most convenient because it needs nothing from you but a frame.
3. **Check the header first** (5 min):
   ```
   .\gb\build\gbemu.exe --rom roms\dmg-acid2.gb --info
   ```
   Confirm the ROM size code and that the declared size is 32 KiB. `--info` also tells
   you the cartridge type — `ROM ONLY` is what you want today.
4. **Run it** (10 min):
   ```
   .\gb\build\gbemu.exe --rom roms\dmg-acid2.gb --frames 600 --dump-frame build\frame.bmp --trace build\trace.txt
   start build\frame.bmp
   ```
5. **Read the trace tail** (5 min). `--frames 2 --trace build\trace2.txt`, then look at
   the last 40 lines: you should see the game's own init code, and a `HALT`-ish loop if
   it has reached its main loop. That is the game *pacing itself on VBlank*, which is the
   thing every real Game Boy program does.

---

## 3. Prove it — 45 min

```
.\gb\build.cmd -Test
```
```
  ...  m00 through m06 all green, m07+ red or skipped
```
Then the real proof, and the reason this lesson exists:

```
.\gb\build\gbemu.exe --rom roms\dmg-acid2.gb --frames 600 --dump-frame build\acid2.bmp
start build\acid2.bmp
```

You will not yet get the *correct* face — that is L21-L24, and acid2 is deliberately
brutal about sprite priority and the window. What you are looking for today is:

* the ROM **runs to frame 600 without faulting**,
* the image is **not blank** and **changes as the ROM animates** (acid2 draws
  progressively),
* the trace shows the ROM's own instruction stream, not yours.

If you have a small 32 KiB homebrew with a title screen, run that instead: it is a better
AHA and it exercises scrolling, sprites and the window in ways acid2's fixed face does
not. Either way, capture the BMP into `NOTES.md` — that image is your baseline, and
every later lesson's progress is diffed against it.

Then spend the remaining time on the *debugging loop itself*, which is the skill this
lesson is really about:

1. `--trace build\t.txt --frames 1`, find the last instruction before the screen stopped
   making sense,
2. dump that address's neighbourhood with `gb_hexdump` or a hex editor,
3. form a hypothesis, change one thing, re-run.

---

## 4. If it fails

| Symptom | Cause | Find it |
| --- | --- | --- |
| `m06_run_60_frames` reports far too many ticks | the PPU is ticked twice per instruction (e.g. from both `bus_tick` and `ppu_tick`'s own loop) | one tick call per step |
| `m06_run_60_frames` never completes | `ppu.frame_ready` is never set, or `gb_step` clears `gb->frame_ready` immediately | the lift in `gb_step` |
| The emulator exits immediately with `UNIMPLEMENTED` | a real opcode is missing; the message names it | that is the loud abort working correctly |
| `--info` says the ROM is bigger than 32 KiB | you need banking | L26, or §6 today |
| The image is blank | the ROM sets `LCDC` but `ppu_tick` is not being called | check `bus_tick` |
| The image is frozen but the ROM runs | the game is waiting on VBlank, so the PPU is not advancing | same check |
| The image is stable but wrong | that is expected today | L21-L24 |
| `main.c` refuses to run | you forgot `--frames` | it requires a stop condition by design |

---

## 5. Done when

- [ ] All five `m06_*` tests are green and `-Test` shows `m00`-`m06` entirely green
- [ ] A real 32 KiB ROM runs 600 frames without faulting
- [ ] You have a BMP on disk produced by that ROM, saved as your baseline
- [ ] `--trace` captures the ROM's instruction stream and you have read its last 40 lines
- [ ] You can name the five calls in one machine step, in order, from memory
- [ ] Commit message like `feat(gb): the platform runs a real ROM end to end (L20)`

---

## 6. Optional, only if you have time

* **Jump ahead to banking.** If you want a commercial game now, L26's substance is about
  30 minutes: MBC1's ROM bank register at `0x2000-0x3FFF` (with the "writing 0 selects
  bank 1" rule), the secondary 2-bit register at `0x4000-0x5FFF`, the mode select at
  `0x6000-0x7FFF` and `ram_enabled` at `0x0000-0x1FFF`. The provided tests
  `m08_mbc1_bank0_quirk` and `m08_mbc1_mode_ram` will check it. Doing it out of order is
  fine; the ladder's order is a default, not a law.
* Add a `--screenshot-every N` flag that dumps a numbered BMP sequence, then turn it into
  a video with `ffmpeg -framerate 60 -i frame%d.bmp out.mp4`. Ten minutes, and it makes
  animation bugs obvious in a way single frames never do.

Next: **[L21 — Sprites](L21-sprites.md)**
