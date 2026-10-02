# L01 — The feedback loop

**Time** — theory ~25 min | coding/setup ~55 min | verify ~10 min
**You will end with** — a build you can trigger with one keypress, a test suite you
can read, and a written answer to "where does state live and who advances the clock".
**Tests that must go green** — `m00_smoke_builds`, `m00_types_and_bits`,
`m00_harness_selfcheck` (they already pass: this lesson is about proving *you* can
run them and read them, not about writing code).
**Depends on** — nothing. `PREREQUISITES.md` items satisfied.

---

## 1. Read this — the whole theory for today

### What a Game Boy is

Five chips sharing one address bus and one 4.194304 MHz clock:

```
   +----------------+   address/data   +---------------------------+
   |  CPU LR35902   +------------------+  BUS                      |
   +----------------+                  |   ROM / cartridge (MBC)    |
                                       |   VRAM 8000-9FFF           |
   +----------------+                  |   WRAM C000-DFFF           |
   |  PPU  (LCD)    +------------------+   OAM  FE00-FE9F           |
   +----------------+                  |   HRAM FF80-FFFE           |
   +----------------+                  |   I/O  FF00-FF7F           |
   | Timer | Joypad |                  +---------------------------+
   | Serial| APU    |
   +----------------+
```

The CPU has **no special powers**. It cannot see VRAM, the cartridge, or the
joypad. It can only read and write addresses. So the bus is the spine of the
emulator, and every component is reachable through it.

### What an emulator is

A **deterministic state machine with a clock**. Not a simulator, not a binary
translator, not "calling functions in the ROM". Same ROM + same initial state +
same input at the same cycle = the same trace, forever. That determinism is your
best debugging tool: you can bisect a bug in time instead of guessing.

### The architecture you are given

One struct holds all mutable state, one function advances the CPU, one function
advances everything else:

```
gb_t                       all machine state (cpu, bus, cart, timer, ppu, joypad, serial, apu)
  gb_step()                u32 cycles = cpu_step(gb);   then bus_tick(gb, cycles);
  cpu_step()               ONE instruction, or one interrupt dispatch; returns T-cycles
  bus_tick()               timer + PPU + serial + APU advance by the same T-cycles
  bus_read/bus_write       the only path from the CPU to any component
```

`gb_create`, `gb_reset`, `gb_step`, `gb_run_frame` and the CLI are written for you
(`gb/src/gb.c`, `gb/src/main.c`). They call **your** functions. That is the seam:
you implement `cpu_step`, `op_execute`, `bus_read`, `bus_write`, `cart_*`,
`timer_tick`, `ppu_tick`, `joypad_read`, `serial_tick`, `apu_tick` — and nothing else.

### Your instrument panel: how the harness reports

```
[       OK ] m00_smoke_builds
[  FAILED  ] m01_header_parse
             src/cart.c:121: UNIMPLEMENTED at src/cart.c:121: cart_load() - M01: ...
[  SKIPPED ] m07_window_position  (write me: ...)
   3 passed,   42 failed,   28 skipped  (73 tests registered)
```

Two kinds of red, and they mean opposite things:

| Message | Meaning |
| --- | --- |
| `UNIMPLEMENTED at file:line` | code you have not written yet. It names the line to type. Expected, not a catastrophe. |
| `expected 0x20, got 0x00` | your code ran and produced the wrong answer. **This is the interesting one.** |

The runner installs a fatal handler, so one missing function marks that one test
failed and the run continues. That is why the suite is a to-do list.

### The tracer

`gb/src/debug.c` keeps a ring buffer of the last 256 instructions (PC, raw bytes,
registers, cycles). `gbemu --trace FILE` dumps it; a fatal error prints the last 16
lines automatically. You will lean on this far more than on a debugger.

> **What matters for the code you are about to write**
>
> Today you write no emulation code, so the thing to internalise is the invariant
> every later lesson depends on:
>
> 1. **Mutable machine state lives only in `gb_t`.** A `static u8 vram[0x2000]`
>    inside `bus.c` makes every test order-dependent and unreproducible. Every
>    `static` in `gb/src/` must be a constant table.
> 2. **`bus_read`/`bus_write` are the only path from the CPU to any component.**
> 3. **`bus_tick` is the only place the clock advances for peripherals**;
>    `cpu_step` is the only place it advances for the CPU.
>
> When a later lesson has you confused about where a value should live, the answer
> is always one of those three sentences.

---

## 2. Your task — 55 min

1. **Check your machine** (2 min).
   ```
   python tools\check_env.py
   ```
   Every `REQUIRED` line must be `[ OK ]`. If not, `PREREQUISITES.md` has the fix.
2. **Build** (3 min). `Ctrl+Shift+B` in VS Code, or:
   ```
   .\gb\build.cmd
   ```
   Expect `gb\build\gbemu.exe` and `gb\build\gbemu_tests.exe`.
3. **Run the three M00 tests** (2 min).
   ```
   .\gb\build.cmd -Test m00
   ```
   Expect exactly 3 tests selected, 3 PASS, and the process exit code 0.
4. **Read the machine** (25 min). Open `gb/include/gb/gb.h` and `gb/src/gb.c` and
   trace the call graph. Write it into `NOTES.md`: who calls `cpu_step`, who calls
   `bus_tick`, who adds to `gb.total_ticks`, who sets `gb->frame_ready`.
5. **Read your instrument panel** (10 min). Open `gb/tests/harness.h` and
   `gb/tests/test_main.c`. Answer in `NOTES.md`, in your own words:
   * how a test registers itself (and why you never edit a list to add one),
   * what the command-line filter matches (`m03` vs `m03_alu` — substring, not prefix),
   * what `TEST_TODO` reports,
   * **what happens to the process when `GB_UNIMPLEMENTED` fires under `-Test`**.
6. **See a failure on purpose** (5 min). Copy `gb/tests/t_m00_smoke.c` to
   `gb/tests/t_zz_scratch.c`, change the last assertion's expected value to
   something wrong, rebuild and run `.\gb\build.cmd -Test m00_harness_selfcheck`.
   Read the output. Then **delete `t_zz_scratch.c`** and confirm the suite is back
   to 3 green.
7. **Write down the memory map** (5 min). From `gb/include/gb/bus.h` (the ownership
   table comment) and `docs/03-memory-and-cartridge.md` §3.2, transcribe the
   address map and the register-ownership table into `NOTES.md`. Transcribing it is
   the point — you will need it from L03 onward.
8. **Take a baseline** (2 min). `.\gb\build.cmd -Test` and record the counts
   (`3 passed, 42 failed, 28 skipped` right now) plus the first red milestone's
   `UNIMPLEMENTED:` line verbatim.
9. **Version control** (3 min). `git init`, write `.gitignore`
   (`lessons/M00` H3 has a template — it is now `milestones/M00-setup-and-harness.md`),
   `git status --short` and read it line by line, then commit.

---

## 3. Prove it — 10 min

```
.\gb\build.cmd -Test m00
```
```
...
[       OK ] m00_smoke_builds
[       OK ] m00_types_and_bits
[       OK ] m00_harness_selfcheck

   3 passed,    0 failed,    0 skipped  (73 tests registered)
ALL GREEN
```

Then, and this is the real verification:

```
.\gb\build.cmd -Test
```

It must **finish** (not crash) and print a summary. If it aborts at the first
missing function, the harness's fatal handler is not installed and you have found a
real bug — report it, do not work around it.

---

## 4. If it fails

| Symptom | Cause | Find it |
| --- | --- | --- |
| `build.ps1 cannot be loaded because running scripts is disabled` | PowerShell execution policy | use `.\gb\build.cmd` (it bypasses the policy for that process) |
| `gcc not found` | `C:\msys64\ucrt64\bin` not on PATH | `PREREQUISITES.md` troubleshooting table |
| The run stops at the first red test | the fatal handler is not installed; only happens if you edited `test_main.c` | `git diff gb/tests/test_main.c` must be empty |
| `-Test m00` reports 0 tests | typo in the filter, e.g. `m00_smoke_build` | the runner prints how many it ran; a green run with 0 tests is a bug in your command |
| VS Code tasks fail with a path error | you opened a parent folder, not `gameboy-course` | File -> Open Folder -> `gameboy-course` |

---

## 5. Done when

- [ ] `python tools\check_env.py` shows every REQUIRED item `[ OK ]`
- [ ] `.\gb\build.cmd -Test m00` is 3/3 green and exits 0
- [ ] A full `.\gb\build.cmd -Test` completes and prints a summary you can state from memory
- [ ] `NOTES.md` contains the `gb_step` call graph, the memory map, and the
      component/register ownership table — written, not pasted
- [ ] You can say what `UNIMPLEMENTED at file:line` means and why it is not a crash
- [ ] `git log --oneline` has one commit; `git diff -- gb/include gb/tests gb/src/gb.c` is empty
- [ ] You can explain, in two sentences, why an unknown opcode must abort loudly
      rather than quietly behave like `NOP`

---

## 6. Optional, only if you have time

* `docs/01-orientation.md` — the same architecture, with the trade-offs and the
  vertical-slice reasoning spelled out.
* Write a 20-line Python script that runs `.\gb\build.cmd -Test` and prints a
  per-lesson pass/fail table. It pays for itself by L05.

Next: **[L02 — Read a real cartridge](L02-read-a-real-cartridge.md)**
