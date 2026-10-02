# L29 — Access-level timing

**Time** — theory ~25 min | coding ~50 min | verify ~15 min
**You will end with** — the clock inside the bus: every memory access advances the
peripherals by 4 T-cycles, while the machine's total time does not change by a single
cycle. AHA: *the same totals, entirely different resolution.*
**Tests that must go green** — `m09_access_cycle_costs`
**Gate** — blargg `instr_timing` and `mem_timing` print `Passed`
**Depends on** — L18 (`bus_tick`), L16 (the timer), everything you have measured so far.
You will edit `gb/src/bus.c`, `gb/src/cpu.c` and `gb/src/opcodes.c`.

---

## 1. Read this — the whole theory for today

### What is actually wrong with instruction-stepped time

Since L05, time has been accounted once per instruction:

```
gb_step:  cycles = cpu_step(gb);  bus_tick(gb, cycles);
```

The peripherals therefore see time in lumps. A 12 T-cycle instruction that reads
`(HL)` and writes it back advances the timer and the PPU by 12 ticks **at the end**, when
on hardware the read happens 4 T-cycles in, the write 4 after that, and only then do the
remaining cycles elapse. `mem_timing` measures exactly that: *which* T-cycle inside an
instruction performs its access. Instruction-level accounting cannot express it, so the
test can only fail.

### The fix: tick where the access happens

```
bus_read (gb, addr):   bus_tick(gb, 4);  ... return the byte
bus_write(gb, addr, v): bus_tick(gb, 4); ... store it
```

Every access is one M-cycle, so 4 T-cycles. Nothing else changes: the CPU still executes
the same instructions in the same order, and the machine still advances by the same total.

### The trap: counting those cycles twice

Now there are two clocks claiming the same time. Before the refactor, `cpu_step` returned
the instruction's full cost and `gb_step` ticked it. After it, the accesses tick
themselves, so `cpu_step` must return only what is **left over**:

```
residual = instruction_total_from_the_table - 4 * (accesses it performs)
```

| Instruction | Total | Accesses | Residual |
| --- | --- | --- | --- |
| `NOP` | 4 | 1 fetch | 0 |
| `LD A,d8` | 8 | fetch + operand | 0 |
| `LD A,(HL)` | 8 | fetch + read | 0 |
| `LD (HL),d8` | 12 | fetch + operand + write | 0 |
| `INC (HL)` | 12 | fetch + read + write | 0 |
| `PUSH BC` | 16 | fetch + 2 writes = 12 | **4** |
| `CALL a16` | 24 | fetch + 2 operands + 2 pushes = 20 | **4** |
| `JP a16` | 16 | fetch + 2 operands = 12 | **4** |
| `RET` | 16 | fetch + 2 pops = 12 | **4** |
| `POP BC` | 12 | fetch + 2 pops | 0 |
| `ADD SP,e8` | 16 | fetch + operand = 8 | **8** |

Most instructions have a residual of exactly 0; a handful have 4; `ADD SP,e8` has 8
because it really does spend two M-cycles doing nothing on the bus. So the natural
implementation is a counter rather than a per-instruction table:

```
cpu_step:
    gb->cpu.ticked = 0;                       /* reset the per-instruction counter */
    u32 total = op_execute(gb, opcode);        /* still returns the table total    */
    return total - gb->cpu.ticked;             /* residual, already-ticked part gone */

bus_read / bus_write:
    bus_tick(gb, 4);
    gb->cpu.ticked += 4;
```

The table stays the single source of truth for totals, the bus provides the phase, and
`gb_step`'s existing `bus_tick(gb, residual)` finishes the instruction. `m09_access_cycle_costs`
measures elapsed time from `gb.total_ticks`, so it passes **only** if no cycle is counted
twice — that is the whole point of that test, and it is why it reads the machine's clock
instead of summing `cpu_step`'s return value.

### What this does *not* fix yet

Access granularity gets the *phase* right at 4 T-cycles, which is enough for the timer,
`DIV`, and most of `mem_timing`. What it does not model is the CPU's **internal cycle
order** inside an instruction — for example that `LD (HL+),A` increments `HL` while the
write is in flight, or that a 16-bit read is followed by an idle M-cycle. Those are the
`mem_timing-2` refinements, and they live in L30. Expect `mem_timing` to pass and
`mem_timing-2` to need one more pass; that progression is normal and the ladder plans for
it.

> **What matters for the code you are about to write**
>
> * `bus_tick(gb, 4)` in **both** `bus_read` and `bus_write`, before or after the actual
>   access (the hardware performs them together; pick one and be consistent).
> * A per-instruction counter (`gb->cpu.ticked` — add the field) that `cpu_step` resets and
>   `bus_read`/`bus_write` add 4 to. `cpu_step` returns `total - ticked`.
> * Do **not** change your opcode table's cycle totals. They are still correct, and
>   `m09_access_cycle_costs` checks 44 of them plus a 32-instruction program.
> * If `gb.total_ticks` doubles, you have both the per-access ticks **and** a
>   `cpu_step` that returns the full total. That is the single most likely mistake here,
>   and the test reports it as "the machine clock advanced 2× T-cycles".
> * `bus_tick` still has to exist as the single entry point to the peripherals — you are
>   calling it more often, not differently.
> * Access-granular ticking makes the PPU and timer far more sensitive to a wrong
>   `bus_tick` argument. If `instr_timing` fails in a *pattern* (always a multiple of 4),
>   the residual arithmetic is wrong; if it fails randomly, a peripheral is being ticked
>   from the wrong place.

---

## 2. Your task — 50 min

Work in `gb/src/bus.c`, `gb/src/cpu.c`, `gb/src/opcodes.c`.

1. **Add the counter** (10 min). A `u32 ticked;` field in `cpu_t`, reset at the top of
   `cpu_step`, incremented by 4 in each bus access.
2. **Tick in the bus** (10 min). `bus_tick(gb, 4)` at the top of `bus_read` and
   `bus_write`. Keep it in one place per function.
3. **Return the residual** (15 min). `cpu_step` returns `op_execute(...) - gb->cpu.ticked`.
   Watch the two exceptions: the interrupt dispatch (still a flat 20, with no accesses)
   and `HALT` (4, no accesses).
4. **Check the totals** (10 min). Run the timing table first, because it tells you
   immediately whether the accounting is right:
   ```
   .\gb\build.cmd -Test m09_access_cycle_costs
   ```
   Then the whole suite — nothing else may change:
   ```
   .\gb\build.cmd -Test
   ```
5. **Run the gate** (5 min) once the unit tests are calm:
   ```
   .\gb\build\gbemu.exe --rom roms\blargg\instr_timing.gb --serial - --frames 2000
   .\gb\build\gbemu.exe --rom roms\blargg\mem_timing.gb  --serial - --frames 2000
   ```

---

## 3. Prove it — 15 min

```
.\gb\build.cmd -Test m09_access_cycle_costs
```
```
ALL GREEN
```
Then the regression sweep, which matters more here than in any other lesson because this
change touches every instruction:

```
.\gb\build.cmd -Test
```
You are looking for the same counts as before the refactor, with only `m09_access_cycle_costs`
changing from red to green. If `instr_timing` or any earlier CPU test regressed, the
residual arithmetic is wrong for a specific instruction shape — the failing test names it.

```
.\gb\build\gbemu.exe --rom roms\blargg\instr_timing.gb --serial - --frames 2000
```
```
instr_timing
Passed
```
`instr_timing` verifies the T-cycle cost of *every* instruction, including both sides of
the conditionals, using the timer as a stopwatch. Passing it means your instruction totals
are right *and* your timer is being clocked correctly by the bus — two things this lesson
touches at once. `mem_timing` is the access-phase test; if it passes too, the phase is
right at 4 T-cycles.

---

## 4. If it fails

| Symptom | Cause | Find it |
| --- | --- | --- |
| `m09_access_cycle_costs` reports exactly double | both the instruction total and the per-access ticks are being counted | return the residual, not the total |
| A specific instruction is 4 or 8 T out | its residual is wrong | count its accesses: fetch, operands, memory, stack |
| Every test in the suite is slightly slow or fast | `bus_tick` is called with something other than 4 per access | one access is one M-cycle |
| `instr_timing` fails in multiples of 4 | the residual subtraction is off by one access for a class of instructions | check the `(HL)` and immediate forms |
| The PPU visibly stutters or tears | the PPU is ticked twice per access, or `gb_step` still adds the full total | `bus_tick` once per access, residual at the end |
| The tracer's cycle column stops making sense | `gb.total_ticks` is no longer monotonic with the instruction log | the clock must advance in exactly one place per cycle |
| `m06_run_60_frames` regresses | the frame boundary moved because time doubled | fix the accounting, do not relax the test |

---

## 5. Done when

- [ ] `m09_access_cycle_costs` is green (all 44 rows plus the 32-instruction program)
- [ ] `-Test` shows the same counts as before the refactor, with only that test newly green
- [ ] `instr_timing` passes, and ideally `mem_timing` does too
- [ ] `gb.total_ticks` for the loop program is still exactly 212
- [ ] You can explain why the residual is 0 for `LD A,(HL)` and 4 for `PUSH BC`
- [ ] Commit message like `feat(bus): access-level ticking with a residual cycle count (L29)`

---

## 6. Optional, only if you have time

* `docs/06-verification-and-tooling.md` §6.3 — how to read a blargg failure number, which
  is what you will be doing for the next hour if `mem_timing` fails.
* Keep the tracer on for one run of `instr_timing` and count the instructions it executes
  by hand for one instruction class. It is the only way to *see* the phase, rather than
  infer it from a pass.

Next: **[L30 — The awkward quirks](L30-the-awkward-quirks.md)**
