# Build a Game Boy emulator

You write the emulator. This repository is everything around it: the theory, the
interface contract, the acceptance tests, the build, the hint ladders, and the
milestone order. There are no solutions in here — that is the point.

Target: the original **DMG** (Game Boy / Game Boy Pocket), in **C17**, on Windows
with **MSYS2 UCRT64 gcc 15.2.0**, plus **Python 3.11** for tooling (any Python 3.8+
works, and nothing needs `pip`). CGB/double speed is an optional final milestone.

**Prerequisites are one file: [PREREQUISITES.md](PREREQUISITES.md).** It lists what
you need now, what you need later, what is deliberately *not* needed (`make`,
`cmake`, MSVC, any `pip` package, a commercial ROM), and how to verify the whole
machine in one command.

## Start here

```
cd gameboy-course

python tools\check_env.py       # confirm the toolchain before trusting anything else
.\gb\build.cmd                  # build gb\build\gbemu.exe + gbemu_tests.exe
.\gb\build.cmd -Test m00        # 3 tests, green before you write any code
.\gb\build.cmd -Test            # everything: 3 green, 72 red, 2 skipped right now
```

**Using VS Code on Windows?** Open the `gameboy-course` folder itself as the
workspace root. `.vscode/` ships eleven tasks (`Ctrl+Shift+B` to build,
`Ctrl+Shift+P` -> "Tasks: Run Task" for the rest) and three gdb launch
configurations. The gdb install and the IntelliSense notes are in
[PREREQUISITES.md](PREREQUISITES.md).

New to Game Boy hardware, or want to know exactly what to read and in what order?
Start with **[docs/00-reading-path.md](docs/00-reading-path.md)**: it maps every topic
to its course doc, its primary source and the test that settles it, lists the
explain-back checks that tell you whether you actually learned it, and says which
sources to clone for offline study.

Then work **[LESSONS.md](LESSONS.md)** top to bottom. **L01** is
[lessons/L01-the-feedback-loop.md](lessons/L01-the-feedback-loop.md). Every lesson
is self-contained (all its theory is inside it), capped at 90 minutes, and ends
with named tests going green. **[TASKS.md](TASKS.md)** is the checklist view;
**[milestones/](milestones)** holds the longer-form briefs and hint ladders for
when you want the bigger picture or you are stuck.

> `.\gb\build.cmd` is the entry point because many Windows machines block `.ps1`
> by execution policy. If your policy allows scripts, `.\gb\build.ps1` works
> identically. On WSL/Linux/macOS use `make` / `make test`.

Right now the suite looks like this, and that is correct:

```
[       OK ] m00_smoke_builds
[  FAILED  ] m01_header_parse
             UNIMPLEMENTED at gb\src\cart.c:121: cart_load() - M01: parse the header
[  SKIPPED ] m07_bg_tile_decode  (write me: ...)
   3 passed,   42 failed,   28 skipped  (73 tests registered)
```

Reading a red run is a skill this course teaches deliberately:
`UNIMPLEMENTED at file:line` means *code that does not exist yet* and names the
line to type. `expected 0x20, got 0x00` means *your code ran and is wrong* — those
are the interesting failures. A run never dies on a missing function, so one red
milestone cannot hide the next one's progress.

## The five ideas that carry the whole project

1. **The bus is the spine.** The CPU cannot see VRAM, the cartridge or the
   joypad; it can only read and write addresses. Funnel *all* access through
   `bus_read()` / `bus_write()` and banking, tracing and timing all fall out of
   one place.
2. **One struct, one clock.** All mutable state lives in `gb_t`. `gb_step()`
   executes one instruction and advances every other component by the same
   number of T-cycles. No component owns hidden state.
3. **Fail loudly.** An unimplemented opcode or register aborts with PC, opcode
   and a register dump (`GB_UNIMPLEMENTED`). A silently-skipped instruction
   corrupts state and shows up as an unrelated hang four minutes later.
4. **Vertical slices.** Make something end-to-end runnable at every step: PC
   advances → a hand-written program runs → `cpu_instrs` passes → a game boots →
   the picture is right → the picture is right *on time*. Never "all 512 opcodes,
   then test".
5. **Test ROMs are the truth.** Your reasoning is the thing under test. Use the
   oracle hierarchy: hardware > test ROM > mature emulator > spec text > you.

## Milestone map

The **lessons** ([LESSONS.md](LESSONS.md)) are the work: 36 self-contained
90-minute units. The **milestones** below are the coarser arc they sit in - 3 to 6
lessons each - and each has a brief in `milestones/` with the full deliverable
contract, work order, trap list and hint ladder. Reach for a milestone when you
want to see where a lesson sits, or when a lesson's "if it fails" section is not
enough.

| # | Milestone | Milestone brief | Tests in the suite | External gate |
| --- | --- | --- | --- | --- |
| M00 | Toolchain, build, harness, architecture | [M00](milestones/M00-setup-and-harness.md) | 3 | — |
| M01 | Cartridge header + address decoder | [M01](milestones/M01-cartridge-and-bus.md) | 6 | `--info` matches a hexdump |
| M02 | CPU skeleton: fetch/decode, first opcodes | [M02](milestones/M02-cpu-skeleton.md) | 6 | — |
| M03 | 8-bit load matrix, ALU, flags, `DAA` | [M03](milestones/M03-8bit-core.md) | 11 | — |
| M04 | Control flow, stack, the whole `CB` block | [M04](milestones/M04-control-flow-and-cb.md) | 7 | — |
| M05 | Interrupts, `HALT`, timers, serial | [M05](milestones/M05-interrupts-timers-serial.md) | 9 | **blargg `cpu_instrs` -> `Passed`** |
| M06 | Frame loop, PPU skeleton, joypad, boot skip | [M06](milestones/M06-first-frame-loop.md) | 5 | a real ROM runs 600 frames |
| M07 | PPU: BG, window, sprites, STAT | [M07](milestones/M07-ppu.md) | 10 | **`dmg-acid2` renders the smiley** |
| M08 | MBC1/2/3/5 banking, save RAM | [M08](milestones/M08-mbcs-and-saves.md) | 5 | a game keeps its save |
| M09 | Access-level timing and quirks | [M09](milestones/M09-timing-accuracy.md) | 5 | **`instr_timing`, `mem_timing`** |
| M10 | APU: four channels to WAV | [M10](milestones/M10-audio.md) | 5 | **blargg `dmg_sound` 01-06** |
| M11 | Save states, disassembler, debugger, window | [M11](milestones/M11-polish-and-debugger.md) | 3 | `--save-state` round trip |
| M12 | Stretch: CGB, or a Python differential fuzzer | [M12](milestones/M12-stretch-cgb.md) | 2 stubs | your own definition of done |

The briefs in [milestones/](milestones) are **planning documents** kept for reference:
longer-form work orders with trap lists and hint ladders, written before the course was
restructured into lessons. The path to follow is the 36 lessons in
**[LESSONS.md](LESSONS.md)**; the table above maps each archived brief to the milestone
number it used and to the tests that now cover it (75 real tests, 2 stubs — sum the
column to check).

A **lesson** has the same eight markers every time: **Time**, *Tests that must go green*,
the "What matters for the code you are about to write" highlight, and then the numbered
sections **1. Read this**, **2. Your task**, **3. Prove it**, **4. If it fails**,
**5. Done when**, plus an optional **6. Optional**. All of a lesson's reading is inside
the lesson, and none of them exceeds 90 minutes. An **archived milestone brief** has the
older shape (Goal, Read first, Why, Deliverable contract, Work order, Acceptance tests,
Common traps, a three-tier hint ladder, Done when, Stretch, Commit) and is where the hint
ladders live: **the hint ladder is the answer key's replacement** — H1 is a question, H2 is
a technique, H3 is the last step before the answer, never code.

## Repository map

```
README.md                  this file
TASKS.md                   the milestone checklist. Your todo list. Start here.
PREREQUISITES.md           required / later / not-needed tools, VS Code setup, troubleshooting
.vscode/                   VS Code tasks, debug configs and IntelliSense for this layout
docs/
  00-reading-path.md       how to learn this: reading order, topic -> source matrix,
                           explain-back checks, schedules, what NOT to read
  01-orientation.md        what an emulator is, the architecture, the debug loop
  02-cpu.md                LR35902: registers, flags (full rules), DAA, interrupts
  03-memory-and-cartridge.md  memory map, boot ROM, header, MBCs, OAM DMA
  04-ppu-and-peripherals.md   modes, timing, tiles, window, sprites, joypad, serial
  05-audio.md              the APU: channels, frame sequencer, mixing
  06-verification-and-tooling.md  oracles, test ROMs, tracing, symptom -> cause
LESSONS.md                 THE WORK: the 90-minute lesson ladder. Start here.
lessons/                   L01..L36, self-contained 90-minute lessons
milestones/                M00..M12 briefs: contracts, work orders, trap lists,
                           hint ladders. Planning documents, not lessons.
reference/
  cheatsheet-opcode-map.md         all 512 opcodes, cycles, illegal list
  cheatsheet-flags-and-timing.md   flag rules, constants, post-boot state
  cheatsheet-io-registers.md       FF00-FF7F with owners and quirks
  external-references.md           verified links + test ROM suites
  glossary.md                      the vocabulary
  tables/                          vendored opcode tables (CC0 / MIT)
gb/
  README.md                build, test and ROM-running commands
  build.cmd / build.ps1    gcc-only build + test (no make/cmake needed)
  Makefile                 for WSL/Linux/macOS
  include/gb/*.h           the contract: structs, signatures, constants
  src/*.c                  GIVEN: main, gb.c, debug.c, screenshot.c
                           YOURS: cpu, opcodes, bus, cart, timer, ppu, joypad,
                                  serial, apu
  tests/                   harness.h + test_main.c (given), t_m00..t_m06 (given,
                           red until implemented), t_m07..t_m12 (you write them)
roms/README.md             where to get legal test ROMs and homebrew
tools/opcode_table.py      generates the opcode map, and checks your C table against it
tools/check_links.py       verifies that every relative link in the course resolves
```

## Given vs yours

**Given (do not edit):** `include/gb/*.h` (the contract), `src/main.c`,
`src/gb.c`, `src/debug.c`, `src/screenshot.c`, `build.cmd`, `build.ps1`,
`Makefile`, `tests/harness.h`, `tests/test_main.c`, and the M00-M06 test files.

**Yours:** every line of `src/cpu.c`, `src/opcodes.c`, `src/bus.c`, `src/cart.c`,
`src/timer.c`, `src/ppu.c`, `src/joypad.c`, `src/serial.c`, `src/apu.c`, the
tests for M07+ if you want them, and `NOTES.md`.

If a provided signature genuinely does not fit, write it in `NOTES.md` and
change the header deliberately — but understand that `git diff -- gb/include`
being empty is what keeps the given tests meaningful.

## The eight rules

1. **One struct.** No file-scope mutable state in a component. Every `static` in
   `gb/src/` is either a constant table or a bug.
2. **One clock.** `cpu_step()` advances the CPU; `bus_tick()` advances
   everything else; `gb_step()` connects them. Do not invent a third path.
3. **Fail loudly.** `GB_UNIMPLEMENTED` for missing features, `cpu_fatal()` for
   guest faults (illegal opcodes). Never treat an unknown opcode as `NOP`.
4. **Never cache an I/O read.** `LY`, `DIV`, `TIMA`, `P1` and `STAT` change under
   you and have side effects on read.
5. **Little endian, everywhere.** Immediates, addresses, `bus_read16`, stack
   pushes.
6. **Flags are derived, not remembered.** Every flag bit comes from a rule in
   `docs/02-cpu.md` §2.4, and every rule has a test.
7. **The tracer is not optional.** Build the ring buffer in M02, add `--trace`,
   and read the last 20 instructions before you change any code.
8. **A milestone is green until it is green *and* its external gate passes.**
   `cpu_instrs` matters more than your own unit tests.

## Definition of done (the whole project)

1. blargg `cpu_instrs` -> `Passed`.
2. blargg `instr_timing`, `mem_timing`, `mem_timing-2` -> `Passed`.
3. `dmg-acid2` -> the correct face, no stray pixels.
4. At least one commercial game boots, is playable, and saves.
5. blargg `dmg_sound` sub-tests 01-06 -> `Passed`.
6. You can explain, without looking, why each of those tests would catch a bug
   your unit tests miss.

## Legal note

You need your own ROMs. Test ROMs and free homebrew are linked in
[roms/README.md](roms/README.md) and
[reference/external-references.md](reference/external-references.md). Commercial
ROMs are not provided, and the boot ROM is copyrighted — skipping it is
supported and is the default (`--no-boot-rom`).
