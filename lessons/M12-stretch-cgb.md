# M12 -- Stretch: colour, or a differential fuzzer, or your own project

**Goal**: pick ONE of three projects that go beyond the milestones, write a definition
of done *before* you start, and finish it. There is no automated gate, but every option
ends with evidence you can show someone else.
**Estimated effort**: 4-10 evenings for option A, 3-6 for option B. Read first; guessing
costs more than reading.

## Read first

* `docs/01-orientation.md` section 1.4 -- determinism, and "diff two implementations".
* `docs/05-audio.md` sections 5.4 and 5.8 -- the timing vocabulary ("T-cycle", "frame
  sequencer", "exactly N T-cycles") that options A and B both need.
* `docs/02-cpu.md` -- option B needs it: you are going to instance two CPU models.
* Pan Docs, for whichever CGB registers you pick; this lesson names them, the bit-level
  detail belongs in the spec, not here.
## Why this milestone exists

Every earlier milestone had a test ROM that said "done"; real emulator work does not work
that way. You decide what done means, build the evidence yourself, and justify the scope:
option A is breadth (a second hardware generation), option B is depth (a machine that
finds *your* bugs), option C is the project you already want to build.

## Deliverable contract

Pick exactly one option and write its definition of done (3-6 checkable statements) into
`TASKS.md` before writing code. All options additionally require: a README section of at
most one page explaining what you built and how to run the evidence; determinism
preserved, so no wall clock or RNG in the core and the fuzzer seed lives in the harness;
no regression, with the baseline output saved before you start; and one commit containing
the code *and* the evidence.

### Option A -- Game Boy Color support

Deliverable: `gbemu` boots a CGB game in CGB mode, with real CGB state not just decoded.

1. **Header flag** at `0x0143`: `0x80` = CGB-capable, `0xC0` = CGB-only. Derive
   `gb.mode` at load time and keep DMG behaviour as the default when the flag is 0.
2. **`KEY0` (FF4C)** and **`KEY1` (FF4D)**: `KEY1` bit 7 reports the current speed and
   bit 0 is the speed-switch request; a `STOP` with that request pending must actually
   change the CPU/APU clock relationship, not just a flag bit.
3. **VRAM bank select (FF4F)**: bit 0 selects bank 0 or 1 for `0x8000-0x9FFF`, reads and
   writes alike.
4. **CGB palette RAM**: `FF68`/`FF69` (BG) and `FF6A`/`FF6B` (OBJ). Index byte: bit 7
   auto-increment, bits 5-3 palette 0-7, bits 2-0 colour 0-3; data is BGR555.
5. **CGB boot state**: skip the boot ROM the CGB way -- different final registers and the
   "something wrote to FF50" behaviour. Use a saved post-boot state.
6. **Per-tile attributes**: the BG map attribute byte lives in VRAM bank 1 at the same
   offset as the tile number in bank 0. Decode palette (bits 0-2), X-flip (5), Y-flip
   (6) and BG-to-OAM priority (7) in the BG renderer; OBJ uses those bits plus bit 3
   (tile VRAM bank).

| Test | Pass criterion |
| --- | --- |
| `m12_cgb_palettes` | index+data writes through `FF68`/`FF69` make a known tile render the written colours; auto-increment advances the index; the `FF6A`/`FF6B` pair behaves identically and independently of the BG pair |
| `m12_double_speed` | after a `STOP`-based switch with `KEY1` bit 0 set, `KEY1` bit 7 reads 1, the CPU executes twice the instructions per emulated frame, and the APU frame sequencer still advances at 512 Hz of real time |

### Option B -- a Python differential fuzzer

Deliverable: a Python CPU model plus a harness that drives the same random opcode streams
through it and through the C core, then reports the *first* divergence.

1. `tools/fuzz.py` generates a random-but-valid opcode stream (random bytes are fine;
   do not let the program run off the end of its test page), seeds identical state into
   both models, and steps both.
2. A Python LR35902 model that is deliberately dumb: registers and flags only, no cycle
   accuracy, no PPU. Write it from the spec, not from your C code.
3. A C entry point that loads a serialised state, executes exactly N instructions and
   dumps the result in a fixed text format (`--fuzz-state/--fuzz-run/--fuzz-dump`), plus
   a comparator reporting the first differing instruction with its full record and a
   corpus under `tests/fuzz-corpus/` so every fixed divergence becomes a committed case.
   Definition of done: it finds a real bug in your C core, the minimal reproducing
   stream is committed, and the corpus reports no divergence.

### Option C -- your own project

Write a definition of done of comparable rigour (3-6 checkable statements) and add it to
`TASKS.md` as `M12-C: <name>`; a headless test runner or a versioned save-state format
with a compatibility check both qualify.

## Work order

1. Choose the option, write the definition of done into `TASKS.md` naming the commands
   that produce your evidence, then save the baseline (full suite plus test ROMs) -- that
   is what "no regression" is measured against.
2. Option A: add `gb.mode` and the header flag first, and verify a DMG ROM is unchanged
   before adding any CGB register.
3. Option A: palette RAM before per-tile attributes (a pure data path, no renderer), then
   `KEY1` before double speed -- a `STOP` that does nothing silently is worse than no
   `STOP` support, because the failure is invisible.
4. Option A: bank select, then BG attributes, then OBJ attributes, rendering a CGB ROM
   after each step; replace the two `TEST_TODO` stubs in `gb/tests/t_m12_stretch.c` as
   soon as each piece works, not at the end.
5. Option B: write the model, then a trivial fuzzer comparing only `A` and `F` after one
   instruction; grow opcode coverage only once the plumbing works. Then fix one real
   divergence end to end and commit the corpus entry.
6. Write the one-page README section, run the full regression suite, then commit.

## Acceptance tests

There is no gate command for this milestone. Option A: replace the two `TEST_TODO` stubs
in `gb/tests/t_m12_stretch.c` so that `gbemu_tests m12_cgb_palettes` and
`gbemu_tests m12_double_speed` pass, and have a named CGB test ROM match a reference
screenshot. Option B: `python tools/fuzz.py --seed N --steps M` reports no divergence on
the committed corpus, and the log of at least one previously found bug is in `TASKS.md`.
Option C: exactly what your written definition of done specifies. In every case the
evidence must be reproducible from a fresh clone using a command copied from the README.

## Common traps

* DMG regressions -> CGB behaviour added without gating on `gb.mode`; re-run the DMG
  test ROMs after *every* CGB change, not at the end.
* Palette writes do nothing -> you clobbered the auto-incrementing index, or wrote data
  before the index; log every palette write and read the index back.
* Double speed doubles *everything* -> you changed the CPU and the PPU, when only the
  CPU/APU ratio changes; track frames per second and APU pitch together.
* Attribute bytes read as tile numbers -> you read VRAM bank 0 at the attribute offset;
  log the bank select on every VRAM access for one frame.
* The fuzzer diverges on its first random byte -> the models disagree on the *initial*
  state; diff the seeded state dump. Only ever finding `NOP` means mostly invalid
  opcodes, or a C core that treats unknown opcodes as NOP instead of aborting loudly.
* Both models are wrong the same way -> the Python was copied from the C; require a cited
  document per line of the model. A fuzzer that takes hours is fuzzing whole programs
  instead of single instructions; 1000 steps should take well under a second.

## Hint ladder

**H1 -- orientation.** Option A: the cheapest visible progress is one colour -- write a
BG palette and render a tile in it. Option B: the cheapest progress is one compared
instruction. Smallest visible effect first, then grow coverage.

**H2 -- structure.** Option A is three layers: mode/header (when), register files
(what), renderer attributes (how it looks) -- verify in that order and guard with
`if (gb.mode == CGB)` at the point of use rather than branching inside shared code.
Option B is likewise model, transport, comparator; one failure must not look like another.

**H3 -- definitions and formulas.** CGB colour is 15-bit BGR555, converted to 8-bit per
channel with `(v << 3) | (v >> 2)`. Palette index byte: bit 7 auto-increment, bits 5-3
palette 0-7, bits 2-0 colour 0-3, two data bytes per colour. BG attribute: the byte at
`0x9800 + n` in VRAM bank 1 describes the tile whose number is at `0x9800 + n` in bank
0 -- bits 0-2 palette, 3 tile bank (OBJ), 5 X-flip, 6 Y-flip, 7 BG-to-OAM priority.
Double speed: CPU/APU divide by 2 while the PPU does not, so the CPU sees twice the
clock per frame with LCD timing unchanged, and `KEY1` bit 7 reflects it. Divergence
record: `PC`, raw opcode bytes, registers before and after, instruction count.

## Done when

1. Your definition of done is in `TASKS.md`, and every statement in it is demonstrated
   or explicitly abandoned with a reason.
2. The evidence is committed and reproducible from the README in one command, and the
   full pre-existing suite plus the previously passing test ROMs still pass.
3. Option A: both `m12_` tests pass and a CGB game boots in colour. Option B: the fuzzer
   found a real bug and the corpus reproduces it. Option C: every statement in your
   definition of done is checkable.
4. You can explain in two or three sentences what you deliberately did *not* implement.

## Stretch

Option A: WRAM bank switching, then HDMA/`FF55` general-purpose DMA, then OBJ priority
resolution. Option B: compare *cycle counts* per instruction (that is where the
interesting bugs are), or fuzz memory access patterns instead of opcodes. Both are the
natural on-ramp to M11's timing work.
## Commit

```
m12: <cgb support | cpu differential fuzzer | your project name>

- <one line per item in the option's definition of done>
- tests: m12_cgb_palettes, m12_double_speed        (option A only)
- evidence: <screenshot | divergence log | corpus> committed
- no regression: full suite and test ROMs re-run
```
