# 06 — Verification: how you will know you are right

## 6.1 The oracle hierarchy

When your emulator disagrees with a document, source, or your intuition, the
ranking is:

1. **Real hardware.**
2. **A test ROM** that was written against hardware.
3. **A mature reference emulator** (SameBoy, Gambatte, BGB) with a known-good
   trace or screenshot.
4. **The spec text** (Pan Docs, gbctr).
5. **Your own reasoning.**
6. A forum post from 2009.

Your own reasoning is the thing under test. That is why the loop is
"compare against an oracle", not "think harder".

## 6.2 Three kinds of test, and when to use each

| Kind | Catches | Cost | Where |
| --- | --- | --- | --- |
| Hand-computed unit tests | flag bugs, decode bugs, off-by-ones | minutes to write, milliseconds to run | `gb/tests/` |
| Test ROMs | anything at all, including things you did not know existed | download + wiring serial | `roms/` |
| Differential testing | table omissions, rare state combinations | hours to build | M12 / Python tooling |

Write unit tests for *everything you can compute by hand*. Use test ROMs for
everything you cannot (timing, PPU priority, long instruction sequences).

## 6.3 Test ROM suites: how to run them

The suite catalogue with URLs and licences is in
`reference/external-references.md`. The mechanics are what matter here.

### blargg's `gb-test-roms`

These report through the **serial port**, which is why you implement serial in
M05 before anything else. Run:

```
gbemu --rom roms\blargg\cpu_instrs\individual\01-special.gb --serial - --frames 4000
```

Expect lines like:

```
01-special
Passed
```

or `Failed #3` (the number tells you which sub-test). Notes:

* The combined `cpu_instrs.gb` runs all eleven sub-tests; some of them are slow.
  If it hangs, run the individual ROMs under
  `cpu_instrs/individual/` and find the first one that fails.
* If serial prints *nothing*, your serial shortcut is wrong, not the CPU. Check
  that a write of `SC = 0x81` emits `SB` and leaves `SC = 0x01`.
* `instr_timing`, `mem_timing`, `mem_timing-2`, `halt_bug` and `oam_bug` are
  separate ROMs in the same repo and are your M09 gate.

### mooneye-gb

Small, sharp tests for one behaviour each. They signal completion by executing
`LD B,B` (opcode `0x40`) as a software breakpoint; the register state at that
moment is the verdict (Fibonacci-looking values for pass, a distinctive failure
value otherwise — read the suite README for the exact protocol). Practical
integration:

```
gbemu --rom roms\mooneye\bits\unused_hwio-C.gb --break-op 40 --frames 5000
```

where `--break-op 40` stops and dumps registers instead of executing past it.
Add that flag in M05; it makes mooneye trivially scriptable.

### dmg-acid2

The PPU acceptance test. It draws a face where each feature exercises one
rendering rule: sprite priority, window positioning, `8x16` mode, palette
selection, the BG-priority bit. Compare your BMP/PPM dump with the reference
image in the repository. It is a *visual* test, not a text one. If the face is
lopsided, that side of the renderer is wrong.

### Also worth having

* `gambatte` test ROMs (PPU and timing heavy).
* `SameBoy` test ROMs (very precise PPU/APU behaviour).
* Anything you write yourself in RGBDS/GBDK once you can assemble.

## 6.4 Writing unit tests that actually help

1. **Data-driven tables.** One loop over an array of cases beats forty functions:

```c
static const struct { u8 a, b, carry, expected_a, expected_f; } cases[] = { ... };
```

2. **Test through the front door.** Put operands in registers, put the opcode in
   memory, run `cpu_step()`, assert the register file. Do *not* call your internal
   `alu_add()` — otherwise you test a function that no opcode actually uses.
3. **Assert the cycles too.** Half of all CPU bugs are cycle-count bugs.
4. **Nothing about time or the host.** A test that depends on the current clock
   will flake.
5. **Print expected vs actual.** `GB_CHECK_EQ(actual, expected)` must show both.
   "assertion failed at line 42" costs you five minutes; the values cost you five
   seconds.

Helper for tests that need a program: `gb_t` with a 32 KiB ROM buffer, write your
instructions at `0x0100`, set `PC = 0x0100`, and `gb_step()` a few times. That is
the whole pattern.

## 6.5 Differential testing with Python — your unfair advantage

You know Python. Use it as a second opinion, not as a second emulator.

```
seed the RNG
for trial in 1..N:
    randomize A F B C D E H L SP, and a page of memory
    pick a random CPU-only opcode (or a short random program)
    run it in C   -> (registers, memory diffs, T-cycles)
    run it in Python -> (registers, memory diffs, T-cycles)
    if any field differs: print opcode, initial state, field, both values; stop
```

Rules that make this pay off:

* Seed the RNG and print the failing case as a ready-to-paste C snippet.
* Compare **every** observable field, including the flag byte and T-cycles.
* Exclude opcodes that touch I/O, the stack, or the PPU at first.
* Let Python also *generate* the expected-value tables you paste into your C
  tests. Two independent derivations, one of which is a machine.

This finds exactly the bugs a hand-written table forgets: `SBC` with carry in,
`ADD SP,e8` flag combinations, `DAA` corner cases, `(HL)` cycle counts.

## 6.6 Tracing and bisecting

* Keep the ring buffer of the last 256 instructions; `--trace FILE` writes them.
* When a bug appears at frame 900, do not single-step from frame 0. Run to frame
  899, dump a save state, then step and watch the first wrong thing happen.
* **Find the first divergence, then stop.** Once any state field is wrong,
  everything after it is noise. Bisecting to the earliest divergence is the
  entire skill of emulator debugging.
* Keep a scratch log per bug: ROM, input, cycle, and the wrong instruction. That
  becomes your regression corpus.

## 6.7 Symptom -> cause

| Symptom | Look here first, in order |
| --- | --- |
| PC cycling inside `0000-00FF`, nothing else happens | interrupt storm (IME/IE/IF garbage), boot ROM never unmapped, wrong post-boot state |
| Game "runs" but the screen never changes | `LCDC.7` never set, PPU not ticked, `frame_ready` never set |
| Hangs before any serial output | an unimplemented opcode (make it abort loudly), wrong ROM bank at `4000`, wrong PC after `CALL`/`RET` |
| Garbled/lopsided sprites | Y/X `+16`/`+8` offsets, priority order, `8x16` selection, palette bit |
| All colours wrong | palette bit extraction, default `BGP`/`OBP0`/`OBP1` |
| Background shifted by a constant | `SCX`/`SCY` applied twice, or not at all |
| Window repeats its first row | you used `LY` instead of the window line counter |
| Test ROM fails immediately with a number | flags (`H`/`C`), `DAA`, or timing |
| Passes in an emulator, fails a test ROM | timing granularity, VRAM access during mode 3, STAT quirks |
| Audio is a buzz | DAC/envelope gating, waveform not reset/triggered, no downsampling |

## 6.8 Anti-frustration protocol

* **45-minute rule.** If you cannot state a hypothesis, stop editing and go read
  the spec section. Write the hypothesis down before you touch code again.
* Never make random edits to a flag computation. Derive it on paper, or write a
  table-driven test that enumerates all 8 combinations.
* When you ask for help (human or AI), include:
  **the last 20 trace lines, the ROM, the milestone, expected vs actual.**
  "It doesn't work" costs you a day; those four lines cost ten minutes.
* Regression discipline: every bug you fix gets a unit test that fails before the
  fix and passes after. Otherwise you will reintroduce it in M09.

## 6.9 What "done" means for the whole project

1. `blargg cpu_instrs` -> `Passed`.
2. `blargg instr_timing`, `mem_timing`, `mem_timing-2` -> `Passed`.
3. `dmg-acid2` -> the correct face.
4. At least one commercial game boots, is playable and saves.
5. `blargg dmg_sound` sub-tests 01-06 -> `Passed`.
6. You can explain, without looking, why each of those tests would catch a bug
   your unit tests miss.

## Sources

* **Test ROM repos** (all free, licences in the linked annotations): blargg's
  `gb-test-roms`; gekkio's `mooneye-test-suite` (MIT) with prebuilt ROMs at
  <https://gekkio.fi/files/mooneye-test-suite/>; mattcurrie's `dmg-acid2` (MIT) and
  `mealybug-tearoom-tests`; SameBoy's and Gambatte's suites.
* **Reference implementations** for the oracle hierarchy in §6.1: SameBoy
  (open source, readable `Core/`), BGB, Emulicious, Gambatte.
* Annotated links, licences, and the exact pass/fail protocol of each suite:
  [../reference/external-references.md](../reference/external-references.md).
* `docs/00-reading-path.md` §0.6 maps each failure symptom to the document that
  explains it.

Next: **[../TASKS.md](../TASKS.md)** for the milestone checklist, and the
**[../reference/](../reference)** cheatsheets for the data you should not retype.
