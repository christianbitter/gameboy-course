# L31 — The halt bug and the long tail

**Time** — theory ~20 min | coding ~40 min | verify ~30 min
**You will end with** — the halt bug implemented exactly, `STOP` handled honestly, and a
list of what is left to chase. This closes phase 3: after today your emulator passes the
main hardware suites, and what remains is genuinely exotic.
**Tests that must go green** — `m05_halt_bug` (L15) and `m05_halt_bug_double_fetch`
**Depends on** — L15 (`HALT`), L30 (the quirks). You will edit `gb/src/cpu.c`.

---

## 1. Read this — the whole theory for today

### The halt bug, precisely

L15 implemented the first half of this rule. The second half is a hardware *bug*, and it
is worth implementing because one test ROM and a few games depend on it.

With `IME = 0` and an interrupt already pending when `HALT` executes:

1. the CPU does **not** halt — it does not wait for anything;
2. the **next opcode fetch does not advance `PC`**, so the byte after the `HALT` is read
   twice.

For a one-byte instruction after `HALT`, the observable is that it **executes twice**:

```
0100: 76      HALT
0101: 3C      INC A      <- executes twice
0102: 00      NOP
```

After `HALT`, `INC A`, `INC A` -> `A = 2`, and `PC` ends at `0x0102` rather than
`0x0103`. That is exactly what `m05_halt_bug_double_fetch` asserts, and it is the crispest
possible statement of the bug.

Implementation, in one place:

```c
u8 cpu_fetch8(gb_t *gb)
{
    u8 v = bus_read(gb, gb->cpu.pc);
    if (gb->cpu.halt_bug) gb->cpu.halt_bug = false;   /* read it again, do not advance */
    else                   gb->cpu.pc++;
    return v;
}
```

`HALT` arms the flag (`gb->cpu.halt_bug = true`) instead of halting, in the case where
`IE & IF & 0x1F` is already non-zero while `IME == 0`. The flag clears itself on the next
fetch, so the effect is exactly one byte deep — which is why the second `INC A` advances
`PC` normally.

Note the interaction with L15's test: `m05_halt_bug` checks the "does not halt" half and
is deliberately silent about the double fetch, precisely so that today's test can own it.
Two tests, two halves of one rule, each with a single clear failure signature.

### `STOP`

`0x10` is a two-byte instruction: the second byte is not an operand and is never used
(it is conventionally `0x00`). On a DMG, `STOP` halts until a **button is pressed**. On a
CGB it is also the speed-switch trigger (L36).

The honest implementation for a DMG-first emulator:

* advance `PC` past two bytes so the instruction stream stays aligned;
* then either model "halted until a joypad press" (correct, and rarely observable) or
  treat it as a halted state like `HALT` (close enough that no test you will run notices).

`cpu_instrs` explicitly **skips** `STOP`, so nothing in the ladder's gates depends on it.
Implement the two-byte length and move on; write a `TODO(L36)` for the speed switch.

### The long tail, and how to find what is left

After L30 you pass the main suites. What remains is a list you should work from the *test
ROMs* rather than from a document, because the tests tell you which of these matter for the
games you actually run:

| Remaining oddity | Where it bites | Oracle |
| --- | --- | --- |
| OAM corruption (16-bit inc/dec on `FE00-FEFF` in the first ~20 cycles of a line) | a handful of games that use `(HL)`-style OAM updates | `oam_bug` sub-tests 1-8 |
| The exact spurious-STAT-interrupt conditions | games doing per-line STAT tricks inside an interrupt handler | `dmg-acid2` is fine; mooneye `stat/` tests |
| The `TMA` write during the reload window | a few timer-driven games | mooneye `timer/` |
| `LY = 153` behaving like an extra VBlank line | games that count lines | mooneye `ppu/` |
| Unused I/O registers reading `0x00` rather than `0xFF` | hardware-detection code | `mooneye/bits/unused_hwio-C` |
| The DIV-write and TAC-write timer glitches | precise timer code | mooneye `timer/` |
| Boot-ROM-specific register values (`DMG0` vs `DMG`) | nothing you will run | `mooneye/acceptance/boot_regs-*` |

The way to work this list is the same as every other lesson: pick an oracle, run it, read
the failure, fix one thing. What has changed by now is that your emulator is good enough
that each remaining failure is a *precise* statement about one corner of the hardware.

### Where the long tail belongs in your notes

Keep a `TODO(hardware)` section in `NOTES.md` listing the tests you have *not* passed and
why. That list, plus your golden `dmg-acid2` frame, is the honest state of the
implementation — and it is what you would hand someone if you asked for help.

> **What matters for the code you are about to write**
>
> * `cpu_fetch8` is the **only** place that advances `PC` for instruction bytes. Put the
>   halt-bug check there, not in `cpu_step`.
> * The flag must **clear itself on the next fetch**, so exactly one byte is read twice. If
>   you clear it anywhere else, the effect lasts too long and `PC` desynchronises.
> * `HALT` arms the flag only when `IE & IF & 0x1F` is non-zero **and** `IME == 0`. With
>   `IME == 1` the interrupt dispatches normally (L15) and there is no bug.
> * `m05_halt_bug_double_fetch` ends at `PC = 0x0102` and `A = 2`. Both are asserted; the
>   intermediate `PC = 0x0101` after the first `INC A` is the *mechanism* and is not
>   asserted, so an implementation that models the bug differently but still executes the
>   byte twice is accepted.
> * `STOP` is two bytes. If you implement it as one, every instruction after it is
>   misaligned and the game will appear to execute nonsense — the tracer will show it
>   immediately.
> * This is the last lesson in phase 3. Do not start a new subsystem from here without
>   choosing it deliberately from L32-L36.

---

## 2. Your task — 40 min

Work in `gb/src/cpu.c`, plus the `STOP` opcode in `gb/src/opcodes.c`.

1. **The halt bug** (20 min). Add the flag to `HALT`'s handler and the check in
   `cpu_fetch8`. Run both halt tests:
   ```
   .\gb\build.cmd -Test m05_halt
   ```
   `m05_halt_bug`, `m05_halt_bug_double_fetch` and `m05_halt_wake` green. Watch the
   *other* CPU tests while you are here — this changes `cpu_fetch8`, which every instruction
   uses:
   ```
   .\gb\build.cmd -Test m0
   ```
2. **`STOP`** (10 min). `0x10`, two bytes, `PC += 2`, then halt (or model the joypad wake if
   you want it). `cpu_instrs` skips it, so keep it simple and note the CGB case for L36.
3. **Run the external oracles** (10 min) that are relevant to today:
   ```
   .\gb\build\gbemu.exe --rom roms\blargg\halt_bug.gb --serial - --frames 4000
   ```
   `halt_bug.gb` is the ROM written for exactly this rule. If your unit test passes and this
   does too, the implementation is right.

---

## 3. Prove it — 30 min (this is the phase gate)

```
.\gb\build.cmd -Test
```
At this point the whole unit suite should be green except `m10`-`m12`'s stubs. Then the
external suites, all in one sitting — this is the phase 3 gate:

```
.\gb\build\gbemu.exe --rom roms\blargg\cpu_instrs\cpu_instrs.gb   --serial - --frames 20000
.\gb\build\gbemu.exe --rom roms\blargg\instr_timing.gb            --serial - --frames 2000
.\gb\build\gbemu.exe --rom roms\blargg\mem_timing.gb              --serial - --frames 2000
.\gb\build\gbemu.exe --rom roms\blargg\halt_bug.gb                --serial - --frames 4000
.\gb\build\gbemu.exe --rom roms\blargg\oam_bug.gb                 --serial - --frames 8000
.\gb\build\gbemu.exe --rom roms\dmg-acid2.gb --frames 60 --dump-frame build\acid2.bmp
python tools\compare_bmp.py build\acid2.bmp build\reference.bmp
```

Record the pass/fail for each in `NOTES.md`. The realistic outcome after L31:

| ROM | Expected |
| --- | --- |
| `cpu_instrs` | `Passed` (since L18) |
| `instr_timing` | `Passed` (L29) |
| `mem_timing` | `Passed` |
| `mem_timing-2` | may still fail — that is `TODO(hardware)` |
| `halt_bug` | `Passed` today |
| `oam_bug` | some sub-tests, not all — that is fine and expected |
| `dmg-acid2` | 0 differing pixels (since L24) |

That table is the honest state of a hobby emulator, and being able to *state* it is the
real deliverable of this lesson.

---

## 4. If it fails

| Symptom | Cause | Find it |
| --- | --- | --- |
| `m05_halt_bug_double_fetch` gives `A = 1` | the flag is armed but never checked in `cpu_fetch8` | one check, at the fetch |
| `A = 3` or `PC` wildly off | the flag is not cleared on the fetch | clear it there, once |
| Every CPU test fails after the change | `cpu_fetch8` now skips the PC increment unconditionally | the flag must be false in the normal path |
| `HALT` with `IME = 1` misbehaves | you armed the flag in the wrong branch | only when `IME == 0` and something is pending |
| Instructions after a `STOP` look like nonsense in the trace | `STOP` advanced one byte | it is two |
| `halt_bug.gb` fails while your unit test passes | the ROM is stricter about the *interaction* with interrupts | read the ROM's failure number, then the trace |
| A previously passing test regressed | `cpu_fetch8` change affects everything | `-Test m0` names it immediately |

---

## 5. Done when

- [ ] `m05_halt_bug`, `m05_halt_bug_double_fetch` and `m05_halt_wake` are green
- [ ] `-Test m0` shows no regression from the `cpu_fetch8` change
- [ ] `halt_bug.gb` passes, or you have a written explanation of exactly which sub-case does not
- [ ] `NOTES.md` has a `TODO(hardware)` list with the tests you have not passed and why
- [ ] You can explain the halt bug in terms of `PC`, not in terms of halting
- [ ] You can name which phase-3 oracle each remaining quirk belongs to
- [ ] Commit message like `fix(cpu): the halt bug's double fetch, and STOP's length (L31)`

---

## 6. Optional, only if you have time

* `docs/06-verification-and-tooling.md` §6.7 — the symptom-to-cause table, which is the
  right way to work the `TODO(hardware)` list.
* Pick the highest-value item from your `TODO(hardware)` list and chase it with the same
  unit-test-first discipline: write a scratch test that fails, fix it, keep the test. That
  is the whole method, and by now you can run it without a lesson.

Next: **[L32 — Save states](L32-save-states.md)**
