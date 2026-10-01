# M09 - Access-stepped timing: bus cycles, the timer, and DMA

**Goal** - Move cycle accounting from "one instruction, then one `bus_tick`" to "one memory access, one 4-T-cycle step" so the timer, OAM DMA and the PPU observe the machine at hardware granularity.

**Estimated effort** - 8-12 h over 3 sessions. Budget half of it for regressions in M02-M08; that is expected, not a sign you did it wrong.

## Read first

- `gb/src/gb.c` - read `gb_step()` line by line. It is the current owner of the clock, and this milestone changes that ownership. Also read the file header: it says you should not need to edit it.
- `gb/src/timer.c` - the M05 model comment and the explicit list of M09 quirks at the top. That list is your task definition.
- `gb/include/gb/timer.h` and `gb/include/gb/bus.h` - the provided fields `div_counter`, `reload_delay`, `overflow_pending`, and the DMA fields `dma_source_high`, `dma_index`, `dma_cycles_left`, `dma_active`. Also `GB_IF(gb)`, `GB_IE(gb)` and the `GB_INT_*` bits.
- `gb/include/gb/common.h` - `GB_TICKS_PER_FRAME`, `GB_INT_VECTOR(bit)`, `GB_UNIMPLEMENTED`.
- `docs/02-cpu.md` 2.3 (cycle accounting) and the opcode table - the authoritative cycle costs.
- `docs/04-ppu-and-peripherals.md` 4.1 and 4.3 for the mode-3 length and the restricted-access rules.
- `docs/03-memory-and-cartridge.md` 3.6 (OAM DMA) and 3.7 (bus traps).
- blargg's `instr_timing` and `mem_timing` ROMs; `docs/06-verification-and-tooling.md` for the serial harness.
- `gb/tests/harness.h` - the fixtures `t_exec`, `t_exec_n`, `t_last_cycles` measure exactly what `m09_access_cycle_costs` needs.

## Why this milestone exists

Instruction-stepped timing is a lie that mostly works: 95% of games never notice that TIMA advances in lumps and that a 16-bit read is atomic. The 5% that do are exactly the games with raster effects and cycle-timed audio, and they are the ones you will be judged by. The hard part is that this milestone is not additive: you are changing the contract between the CPU, the bus and every peripheral at once, and M02-M08 can all regress on the same afternoon. The reward is that "the screen tears" and "the music runs fast" become one-off-line measurement problems instead of mysteries. Skip it and you will spend M10-M12 chasing symptoms of a clock that does not exist.

## Deliverable contract

You own `gb/src/timer.c` (behaviour), the access sites in `gb/src/cpu.c`/`gb/src/opcodes.c`, and the FF04-FF07 decode plus `bus_tick` in `gb/src/bus.c`.

```c
void timer_tick(gb_t *gb, u32 tcycles);   /* advance DIV and TIMA */
void timer_reset(gb_t *gb);
void bus_tick  (gb_t *gb, u32 tcycles);   /* still the single entry point for time */
```

`timer_t` is provided; fill it, do not extend it first:

```c
u16 div_counter;     /* the whole 16-bit divider, not just the visible byte */
u8  tima, tma, tac;  /* FF05, FF06, FF07 */
u32 reload_delay;    /* M09: 4-cycle window between overflow and the TMA reload */
bool overflow_pending;
```

The structural change, as a rule rather than as code:

```
today:  gb_step() { cycles = cpu_step(gb); ...; bus_tick(gb, cycles); total_ticks += cycles; }
        -> time is spent once per instruction, after the fact

target: cpu_step() performs its accesses in real order and never advances the clock itself
        operand fetch / read / write   ->  bus access, then bus_tick(gb, 4)
        internal cycle (no address)    ->  bus_tick(gb, 4) alone
        interrupt dispatch             ->  20 T-cycles total, 5 steps of 4
```

Ordering rule, pick it once and never deviate: the access takes effect first, then the 4-cycle step is spent, so the timer and PPU see the post-access state on the next step. Mixing before/after across helpers is where off-by-four bugs come from; if a test disagrees, flip the rule globally and rerun, never patch one call site.

**Coordination flag.** Achieving that means deleting the `bus_tick(gb, cycles)` line from `gb_step()` in `gb/src/gb.c`, which the file header marks as provided infrastructure. M09 is the one milestone where that line must go, and the invariant is that time is spent exactly once: either the access helpers tick or `gb_step` ticks, never both. Confirm the exact shape with the lead before editing `gb.c`, and keep `gb->total_ticks` and the tracer's per-instruction `cycles` correct in whichever split you choose.

Timer model, made explicit because `timer.c`'s comment can be read two ways: the 16-bit counter advances **once per T-cycle**, DIV (`FF04`) is its high byte, so DIV advances every 256 T-cycles = 16384 Hz. The TAC bit taps below only produce the listed rates under that reading. Writing DIV clears the whole counter.

| TAC bits 1:0 | TIMA rate | counter bit that clocks TIMA |
| --- | --- | --- |
| 00 | 4096 Hz | 9 |
| 01 | 262144 Hz | 3 |
| 10 | 65536 Hz | 5 |
| 11 | 16384 Hz | 7 |

TIMA increments on the **falling edge** of the selected bit while TAC bit 2 is set. On overflow: TIMA reads 0 for 4 T-cycles, `GB_INT_TIMER` is raised in `GB_IF(gb)`, then TIMA is reloaded from TMA. Which value wins when TMA is written inside those 4 cycles is a documented quirk - `timer_t.reload_delay` and `overflow_pending` exist for it, so read the spec pointer and pin the direction with a test rather than guessing.

Expected totals for `m09_access_cycle_costs` (T-cycles; docs/02's table is authoritative if any row disagrees):

| instruction | T | | instruction | T |
| --- | --- | --- | --- | --- |
| NOP | 4 | | CALL n16 | 24 |
| LD r,r' | 4 | | RET | 16 |
| LD r,n8 | 8 | | PUSH rr | 16 |
| LD A,(HL) / LD (HL),A | 8 | | POP rr | 12 |
| LDH (n),A / LDH A,(n) | 12 | | ADD HL,rr | 8 |
| LD A,(n16) / LD (n16),A | 16 | | INC rr / DEC rr | 8 |
| JP n16 | 16 | | CB-prefixed on r | 8 |
| JR e8 | 12 | | CB-prefixed on (HL) | 12 |

OAM DMA: the copy is M01's work; the 160-M-cycle stall is yours. Writing FF46 sets `dma_source_high`, and the transfer of 160 bytes from `XX00-XX9F` to `FE00-FE9F` takes 160 M-cycles = 640 T-cycles. Drive `dma_index` and `dma_cycles_left` from `bus_tick`, keep the CPU stalled except for HRAM accesses (`0xFF80-0xFFFE`), return 0xFF for everything else while `dma_active`, and restart cleanly on a second FF46 write.

## Work order

1. Make the clock observable before changing it: add a tick counter incremented wherever time is currently spent, and a test that runs one instruction and asserts the count. You want that number before you touch the plumbing.
2. Move the time: put `bus_tick(gb, 4)` in the fetch/read/write helpers and in every internal cycle, and remove the one in `gb_step()`. Grep to prove there is no second owner. Run `gbemu_tests m09_access_cycle_costs`.
3. Rerun every M02-M08 filter and fix the regressions before touching the timer.
4. Timer core: DIV counter, TIMA/TMA/TAC decode, falling-edge clocking, the 4-cycle overflow/reload delay, and `overflow_pending`. Run `m09_timer_overflow_delay`.
5. DIV and TAC write edges, including the increment when the selected bit is high at the moment of the write, plus the DIV-derived edge the APU frame sequencer uses. Run `m09_div_apu_edge`.
6. PPU in dot units: with per-access timing, mode boundaries land mid-instruction. Recheck `m07_stat_modes_timing` and `m07_lyc_coincidence`, then add the mode-3 length and the mode-3 VRAM/OAM restrictions (docs/04 4.3).
7. OAM DMA: the stall, the HRAM window, 0xFF outside it, and restart. Run `m09_oam_dma_timing`.
8. STAT interrupt as the OR of enabled sources with a rising-edge latch (`ppu.stat_line`) plus the write-blocking quirk. Run `m09_stat_blocking`.
9. Interrupts: the EI delay, HALT wake-up, and the 20 T-cycle dispatch using `GB_INT_VECTOR(bit)`. Then run blargg `instr_timing`.
10. `mem_timing`, then one full regression pass over every filter from M02 onwards, tracer on.

Keep the tracer enabled for this whole milestone. When a total is wrong by 4, the trace's per-instruction `cycles` column tells you which instruction, which is 95% of the work.

## Acceptance tests

```
gbemu_tests m09_                 # add tests/t_m09_timing.c; expect "ALL GREEN", exit 0
gbemu --rom instr_timing.gb --serial - --frames 3000     # expect "Passed"
gbemu --rom mem_timing.gb  --serial - --frames 3000      # expect "Passed"
gbemu_tests m0                   # full regression, M02..M08 filters
```

Tests the student must write: `m09_access_cycle_costs`, `m09_timer_overflow_delay`, `m09_div_apu_edge`, `m09_oam_dma_timing`, `m09_stat_blocking`.

Pass criteria: all five PASS with `ALL GREEN`; blargg `instr_timing` and `mem_timing` print their pass banner through `--serial -`; every earlier filter still passes in the same binary; a fixed trace of 1000 instructions has the same total cycle count before and after the change.

## Common traps

- Two clocks. Symptom: the machine runs about twice as fast, audio drifts, `instr_timing` reports roughly half the expected cycles. Cause: `gb_step()` still calls `bus_tick(gb, cycles)` while the access helpers also tick 4. Detect: assert the tick count after a known instruction; grep for `bus_tick` and make sure there is exactly one owner.
- `cpu_step()` returning cycles that a caller spends again. Symptom: over-counting only on some paths. Cause: a return value kept alive after the model changed. Detect: the return value is unused by a caller; per-instruction totals are wrong by a constant.
- Missing internal cycles. Symptom: `JR`, `CALL`, `RET`, `PUSH`, `LD (nn),SP`, `ADD SP,e8` come out 4 T-cycles short. Cause: counting only bus accesses and forgetting the dummy read cycle. Detect: `m09_access_cycle_costs`.
- TIMA reload modelled as instant. Symptom: code polling TIMA sees 0x42 one M-cycle early. Cause: writing TMA into TIMA in the same cycle as the overflow. Detect: `m09_timer_overflow_delay` reads TIMA inside the 4-cycle window.
- TIMA clocked on a level instead of a falling edge. Symptom: TIMA runs at exactly double or half rate after a clock-select or TAC write. Cause: testing the selected bit instead of remembering its previous value. Detect: `m09_div_apu_edge`; log counter, selected bit and TIMA per M-cycle.
- OAM DMA with the wrong duration or no HRAM window. Symptom: DMA-heavy games lose the top of the screen or freeze for a frame. Cause: 160 T-cycles instead of 640, or letting the CPU read normal memory during the transfer. Detect: `m09_oam_dma_timing`; measure frame time.
- Bus reads outside HRAM returning real memory during DMA. Symptom: the game reads garbage where hardware gives 0xFF, usually on the exact frame it starts a DMA. Cause: DMA handled as a background copy instead of a bus state. Detect: assert in `bus_read` when `dma_active` and the address is not HRAM.
- STAT treated as a per-mode-change edge instead of a level built from enabled sources. Symptom: mode 0 and LYC both enabled fire twice per line, or a STAT write spuriously triggers. Cause: no `stat_line` latch and no write-blocking window. Detect: `m09_stat_blocking`.

## Hint ladder

### H1

- Where in the code is "4 T-cycles" spent today? `gb_step()` is one candidate; the CPU helpers are the other. If both spend it, which one must go, and what invariant does that give you?
- Your opcode table says `LD A,(HL)` costs 2 M-cycles. Which of those cycles is the fetch, which is the read, and what does the CPU do during `JR`'s third cycle if it does not touch memory?
- `DIV` is a readable register. What is it a view of, and what does writing it reset?

### H2

- Technique: instrument the clock before refactoring it. One counter plus one assertion lets you change the plumbing with a pass/fail signal at every step. Then land the CPU move and the timer in two separate commits.
- Fixture technique: `t_exec()` runs one instruction and leaves the total in `t_last_cycles`; `t_exec_n(gb, n)` totals a run. `m09_access_cycle_costs` is a table-driven loop over those, not a special-purpose emulator.
- Test-ROM technique: prefer mooneye's per-case ROMs (`div_timing`, `tima_*`, `oam_dma_*`) while iterating; each isolates one behaviour and prints via serial in under a second.
- Recording technique: keep a results table in the repo (ROM, expected, actual, date). You will run these again after every later milestone.

### H3

- The counter is 16-bit and ticks per T-cycle, so `DIV = (div_counter >> 8) & 0xFF` and writing DIV clears `div_counter` to 0. If the timer is enabled and the selected bit is 1 at that moment, TIMA increments once - that is `m09_div_apu_edge`, and the same applies to enabling the timer on such a cycle.
- TIMA increments on the falling edge of `div_counter` bit {00: 9, 01: 3, 10: 5, 11: 7}. On overflow: TIMA reads 0 for 4 T-cycles, `GB_INT_TIMER` (0x04) is raised, then TIMA is reloaded from TMA; a TMA write inside that window has documented-but-surprising behaviour, so check the spec before you decide which value loads.
- OAM DMA is 640 T-cycles for 160 bytes; HRAM is `0xFF80-0xFFFE`; source `XX00-XX9F`, destination `0xFE00-0xFE9F`. Interrupt dispatch is 20 T (2 internal, 2 pushing PC, 1 jumping to `GB_INT_VECTOR(bit)`).

## Done when

- `gbemu_tests m09_` reports 5/5 passing and prints `ALL GREEN`.
- blargg `instr_timing` prints its pass banner via `--serial -`.
- blargg `mem_timing` passes all subtests, not just the first.
- Every filter from M02 to M08 passes in the same binary run.
- The tracer's per-instruction `cycles` column matches docs/02's table and PC never jumps to an unexpected address.
- Exactly one piece of code spends time; `bus_tick` call sites are the access helpers plus `gb_step`'s decision, not both.

## Stretch

- DIV-write and TAC-write increments of TIMA verified against `div_timing`, including the write-while-enabled case.
- The HALT bug: `HALT` with IME=0 and a pending interrupt leaves PC un-incremented, so the next byte executes twice. The `halt_bug` field already exists in `cpu_t`.
- The DMG OAM bug (a DMA started during mode 0 or 2 corrupts OAM rows; the `oam_bug` ROM defines the pattern) plus a mooneye results table kept in the repo as a regression gate for M10-M12.

## Commit

`git commit -am "timing: access-stepped bus, DIV/TIMA edges and reload delay, OAM DMA stall, STAT line; instr_timing+mem_timing pass"`
