# M00 - Setup and the harness

**Goal** - Prove the whole loop (edit -> build -> run tests -> read the failure) works on this machine, and internalise the one-machine-struct design, without writing a single line of emulation code.

**Estimated effort** - 1-2 h, most of it Windows PATH archaeology and reading provided source.

## Read first

- `docs/01-orientation.md` sections 1.1 (the bus is the spine), 1.5 (vertical slices), 1.6 (fail loudly), 1.7 (build the tracer first).
- `docs/06-verification-and-tooling.md` - it owns the test runner, the serial-output mechanics and the test-ROM workflow. Read it before you run anything; do not paraphrase it into your own rules.
- `gb/README.md` and the repo `README.md` - the canonical build commands. They are repeated below; do not invent variants.
- `gb/include/gb/common.h` - `u8/u16/u32`, `GB_FLAG_*`, `GB_TICKS_PER_FRAME`, `GB_INT_*`, `GB_UNIMPLEMENTED`.
- `gb/include/gb/gb.h` - `struct gb_s`, `void gb_step(gb_t*)`, `void gb_run_frame(gb_t*)`, and the list of functions that are yours.
- Every other header in `gb/include/gb/` (`cpu.h`, `bus.h`, `cart.h`, `timer.h`, `ppu.h`, `joypad.h`, `serial.h`, `debug.h`). Structures are provided; behaviour is yours.
- `gb/tests/harness.h` - `TEST(name)`, `TEST_ASSERT`, `TEST_EQ`, `TEST_TODO`.
- External: bookmark <https://gbdev.io/pandocs/> and <https://gbdev.io/gb-opcodes/optables/>. You will live in both.

## Why this milestone exists

The first real risk in this project is not the CPU, it is the feedback loop: a wrong `gcc` on `PATH`, a blocked PowerShell script, or a filter that silently selects zero tests will waste an evening and look like an emulator bug. The second risk is architectural. `gb_t` is the only place mutable machine state may live; if a `static u8 wram[0x2000]` appears in `bus.c` in M01, every unit test from then on becomes order-dependent and unreproducible. M00 freezes both: repeatable toolchain, and an explicit check that no component owns hidden state. It also teaches you to read the test output correctly, because for the next six milestones most of your suite is *supposed* to be red: the runner catches `GB_UNIMPLEMENTED`, marks that one test FAILED with `UNIMPLEMENTED: <file>:<line>`, and keeps going. "34 failed, 3 passed" is a progress bar, not a catastrophe. Finally, M00 is your only chance to read all the provided code once, in order, before you are tempted to skim it.

## Deliverable contract

No emulation code. You produce:

- A git repository whose root is the `gameboy-course/` directory (the one holding `gb/`, `docs/`, `lessons/`), with `.gitignore` and a first commit. All paths in this course are relative to that root.
- `NOTES.md` at that same root: your append-only decision log. It must contain, in your own words, the toolchain versions, the 64 KiB memory map, the `gb_t` component list, the first RED milestone, and a failure count per milestone.
- An unmodified provided tree: `git diff -- gb/include gb/tests gb/build.ps1 gb/src/gb.c` stays empty through M06.
- The three M00 tests green: `m00_smoke_builds`, `m00_types_and_bits`, `m00_harness_selfcheck`. They already pass; your job is to prove you can run them and explain what each one asserts. Read `gb/tests/t_m00_smoke.c` and summarise it in `NOTES.md` - "it passed" is not an answer.

The state invariant you are certifying, in one line: **mutable machine state lives only in `gb_t`; component `.c` files own behaviour, never data.**

```c
/* shape of every test you will write from M01 on */
gb_t *gb = gb_create();
gb_reset(gb);
/* poke state through bus_write / cpu fields, call one function, assert */
gb_destroy(gb);
```

`timer_reset`, `ppu_reset`, `serial_reset`, `joypad_reset`, `cpu_reset` ship with working minimal bodies (a `memset` of their component). Treat them as reset hooks you may extend as fields appear - not as work items for M00. If `gb_create()` ever aborts, the bug is yours, not the skeleton's.

## Work order

1. `where.exe gcc`, `gcc -v`, `gcc -dumpmachine`, `python --version`. Expect MSYS2 UCRT64 gcc 15.2.0 and Python 3.11, and `x86_64-w64-mingw32` from the dump target. Paste the four outputs into `NOTES.md`.
2. `.\gb\build.cmd -Clean` then `.\gb\build.cmd`. Expect `gb\build\gbemu.exe`. Run `.\gb\build\gbemu.exe --help` and confirm the CLI in `docs/01-orientation.md` (it is the fixed interface for the rest of the course).
3. `.\gb\build.cmd -Test m00`. Expect exactly 3 tests selected, 3 PASS. If the runner reports a different number, stop and re-read your filter.
4. `.\gb\build.cmd -Test` (everything). Record per-milestone pass/fail counts in `NOTES.md`. Expect `m01_`..`m06_` FAILED with `UNIMPLEMENTED:` messages and later milestones reported as skipped/`TODO`.
5. Open `gb/tests/harness.h` and `gb/tests/test_main.c`. Answer in `NOTES.md`: how a test registers itself, what the filter matches (it is a SUBSTRING match, not a prefix), what `TEST_TODO` reports, and what happens to the process when `GB_UNIMPLEMENTED` fires under `-Test`.
6. Read `gb/include/gb/gb.h` and `gb/src/gb.c` end to end. Trace `gb_step()`: who calls `cpu_step()`, who calls `bus_tick()`, who updates `gb.total_ticks`. Write the call graph in `NOTES.md` - every later timing bug is a disagreement with this graph.
7. Read the remaining headers and write the component/register ownership table into `NOTES.md` yourself (FF00 joypad, FF01/02 serial, FF04-FF07 timer, FF0F IF, FF40-FF4B PPU, FFFF IE).
8. Check the state invariant: search `gb/src` for file-scope mutable definitions. Record every hit and why it is either constant or acceptable. Do not "fix" provided files in this milestone.
9. Write `.gitignore`, `git init`, `git add -A`, inspect `git status --short` line by line before committing (a wrong ignore rule is easier to see now than after 40 ROMs land).
10. Commit. Then run `.\gb\build.cmd -Test` one more time from a clean tree and confirm the counts match step 4.

## Acceptance tests

```
.\gb\build.cmd                        # -> gb\build\gbemu.exe
.\gb\build.cmd -Test m00              # -> the three M00 tests
.\gb\build.cmd -Test                  # -> full suite; read the counts
.\gb\build\gbemu.exe --help           # -> the fixed CLI
```

Pass criterion: `.\gb\build.cmd -Test m00` reports `m00_smoke_builds`, `m00_types_and_bits`, `m00_harness_selfcheck` PASS and the runner exits successfully; the full run completes with a summary you can state from memory; `gbemu --help` lists `--rom --info --frames --max-cycles --dump-frame --ppm --serial --trace --no-boot-rom --headless`.

## Common traps

- Wrong compiler. Symptom: link errors about `libgcc`, or `stdint.h` missing, on a machine where gcc clearly exists. Cause: another `gcc` (WSL, an old MinGW, clang) earlier on `PATH` than `C:\msys64\ucrt64\bin`. Detect: `where.exe gcc` lists every candidate; `gcc -dumpmachine` must print `x86_64-w64-mingw32`.
- Script execution blocked. Symptom: `build.ps1 cannot be loaded because running scripts is disabled on this system`. Cause: the default PowerShell execution policy, not the script. Fix: use the `.cmd` wrapper - `.\gb\build.cmd -Test` runs `build.ps1` with `-ExecutionPolicy Bypass` for that one process and needs no machine-wide setting. By hand the equivalent is `powershell -NoProfile -ExecutionPolicy Bypass -File .\gb\build.ps1 -Test`.
- A filter that selects nothing. Symptom: a green run that took 0.01 s. Cause: `m00_smoke_build` (missing `s`) matches no test; the runner has nothing to fail. Detect: read the selected/ran counts on every run; a green run with a count of 0 is a bug in your command.
- Mistaking `UNIMPLEMENTED` for a crash. Symptom: you conclude the skeleton is broken on your first full run. Cause: `GB_UNIMPLEMENTED` is the expected state for M01-M06 code that does not exist yet; the harness installs a fatal handler so the run continues. Detect: the message form `UNIMPLEMENTED: <file>:<line>`; each one names a function you have not written.
- Hidden state. Symptom: tests pass one at a time and fail in sequence, or results change with filter order. Cause: file-scope mutable state in a component `.c` (the classic is a `static` VRAM or a global `gb_t`). Detect: grep for file-scope definitions in `gb/src/*.c` and for any `static gb_t`; the fix is always "put it in `gb_t`".
- `.gitignore` that eats evidence. Symptom: a reference BMP or a golden trace cannot be committed; or `build/` is ignored but `gb/build/` is not (or vice versa). Cause: rules written from memory instead of from `git status` output. Detect: `git check-ignore -v <path>` for every path you expect to commit, and keep ROMs out of the repo while keeping small golden artifacts in.
- Files saved in the wrong encoding. Symptom: `error: stray '\377' in program` on a file you just wrote. Cause: an editor saved the `.c` as UTF-16 (Notepad "Unicode") instead of UTF-8. Detect: `Format-Hex gb\src\cpu.c | Select-Object -First 1` shows a `FF FE` byte-order mark; re-save as UTF-8.
- Editing a provided file to make something compile. Symptom: your change vanishes when the framing is regenerated, or a provided test stops compiling after an update. Cause: you modified `gb/include`, `gb/tests`, `gb/build.ps1` or `gb/src/gb.c`, which you do not own. Detect: `git diff -- gb/include gb/tests gb/src/gb.c` must be empty; if a signature genuinely does not fit, write it in `NOTES.md` and raise it instead of patching the header.

## Hint ladder

### H1

- Which function in `gb/src/gb.c` adds to `gb.total_ticks`, and which one returns the T-cycles that get added? If two places could add the same cycles, which one wins?
- Why does every test construct its own `gb_t` instead of sharing one? What would a shared instance do to a test that fails halfway?
- What does the runner print for a failing test, and where does it tell you the line number? Try it deliberately: break a `TEST_EQ` in `t_m00_smoke.c` in a scratch copy, run it, read the output, revert.

### H2

- Technique: read `gb/src/gb.c` in this order: `gb_create`, `gb_reset`, `gb_step`, `gb_run_frame`. That is the whole machine lifecycle; everything else is a component hanging off it.
- Technique: keep a `NOTES.md` line per milestone of the form `M03: 9 failed / 1 passed -> first failure m03_alu_add_flags: UNIMPLEMENTED src/opcodes.c:41`. The delta between runs is your progress metric when the suite is mostly red.
- Spec/tooling pointers: `docs/06-verification-and-tooling.md` for the runner and test-ROM mechanics, `gb/README.md` for commands, `docs/03-memory-and-cartridge.md` section 3.2 for the memory map you transcribe into `NOTES.md`.

### H3

- Toolchain target: MSYS2 UCRT64. Install the `mingw-w64-ucrt-x86_64-gcc` package, put `C:\msys64\ucrt64\bin` ahead of everything else on `PATH`, and confirm with `gcc -dumpmachine` -> `x86_64-w64-mingw32`. No make/cmake is needed on this machine; `build.ps1` is gcc-only by design.
- `.gitignore` starting point (adjust after reading `git status`):

  ```
  gb/build/
  *.exe
  *.o
  *.obj
  *.bmp
  *.ppm
  *.sav
  *.trace
  roms/
  ```

  Then protect anything you *do* want (a small golden frame, a hand-written test ROM) with an explicit `!path` rule.
- Register/state ownership, the one line you should be able to recite: `gb_t` holds all state; `bus_read`/`bus_write` are the only path from the CPU to any component; `bus_tick` is the only place the clock advances.

## Done when

- `.\gb\build.cmd -Test m00` reports 3/3 PASS, and you can state what each of the three tests asserts without opening the file.
- A full `.\gb\build.cmd -Test` completes, and `NOTES.md` records the pass/fail counts plus the first RED milestone and its `UNIMPLEMENTED:` line verbatim.
- `git log --oneline` shows an M00 commit; `git status --short` is clean apart from intentionally untracked ROM files.
- `NOTES.md` contains the memory map, the `gb_t` component list, the `gb_step` call graph and the register-ownership table, written from understanding, not copied tables.
- `git diff -- gb/include gb/tests gb/build.ps1 gb/src/gb.c` is empty.
- You can explain, in two sentences, why an unimplemented opcode must abort loudly rather than fall through to NOP.

## Stretch

- Write a Python 3.11 script that runs `.\gb\build.cmd -Test`, parses the runner output, and prints a per-milestone progress table (`M01 0/6, M02 0/6, ...`). Run it after every build. It will pay for itself before M03.
- Write a second script that extracts the first `UNIMPLEMENTED: <file>:<line>` per run and opens that location. Removing one round trip per fix matters when there are hundreds.
- Take a real `.gb` file, run `.\gb\build\gbemu.exe --rom <file> --info`, and hand-verify the printed title and ROM size against a hexdump of offsets `0x0134` and `0x0148`. This is your first look at the header you implement in M01.

## Commit

```
chore: init repo, verify MSYS2 UCRT64 toolchain and test harness (M00 green)
```
