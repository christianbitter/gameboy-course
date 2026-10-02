# M02 - CPU skeleton

**Goal** - Build the register file plumbing, the fetch/decode loop and the opcode dispatch table, and make `cpu_step()` return exact T-cycles for a handful of instructions while every unimplemented opcode aborts loudly.

**Estimated effort** - 6-10 h over 2 sessions (dispatch table and fetch ~2 h, first opcodes ~2 h, getting the trace and the abort semantics right ~3 h).

## Read first

- `docs/02-cpu.md` sections 2.0-2.3 (instruction model, cycle accounting) and 2.5 (PC semantics), then 2.7 (table structure). It owns the full cycle and flag rules; do not re-derive them.
- `docs/01-orientation.md` sections 1.2 (the one loop) and 1.6 (fail loudly).
- `gb/include/gb/cpu.h` - `cpu_t`, the inline 16-bit accessors, `CPU_FLAG`/`CPU_SET_FLAG`/`CPU_CLR_FLAG`, and the seven signatures below.
- `gb/include/gb/common.h` - `GB_FLAG_*`, and the difference between `GB_UNIMPLEMENTED` and `cpu_fatal`.
- `gb/include/gb/debug.h` - `gb_trace_record()`, `gb_trace_dump()`. The tracer already exists; you must feed it.
- `gb/src/gb.c` - `gb_step()`: the caller that consumes your return value. Read it before you write `cpu_step`.
- `reference/cheatsheet-opcode-map.md` - the T-cycle table and the eleven illegal opcodes.
- External: Pan Docs "CPU Instruction Set"; <https://gbdev.io/gb-opcodes/optables/> for the encoding patterns.

## Why this milestone exists

Every later milestone is an opcode added to the table you build here, so the table shape is a decision you make once. Get it wrong and M03 is 128 hand-written functions and M04 is unmaintainable; get it right and `0x40-0x7F` and `0x80-0xBF` collapse into two loops of eight. M02 is also where the cycle contract becomes real: `cpu_step()` returns T-cycles and `gb_step()` is the only thing that forwards them to `bus_tick()`. If you let `cpu_step()` advance the peripherals itself, timings double-count, `DIV` runs at the wrong rate, and every timing test fails later for a reason that looks like a PPU bug. Finally, M02 establishes the failure policy that saves the whole project: eleven opcodes are architecturally illegal and get `cpu_fatal()` (a testable guest fault), while every opcode you have simply not written yet gets `GB_UNIMPLEMENTED` (a loud bug). Falling through to NOP instead is how a one-register corruption becomes a four-minute hang.

## Deliverable contract

`gb/src/cpu.c` (and `gb/src/opcodes.c` if you keep the tables there):

```c
void cpu_reset(gb_t *gb);                      /* extend the given skeleton */
u32  cpu_step (gb_t *gb);                      /* returns T-cycles */
u8   cpu_read_r8 (gb_t *gb, int idx);          /* idx 0..7 = B C D E H L (HL) A */
void cpu_write_r8(gb_t *gb, int idx, u8 value);
u8   cpu_fetch8 (gb_t *gb);                    /* bus_read(pc++), 2 lines */
u16  cpu_fetch16(gb_t *gb);                    /* two fetch8, little endian */
```

Obligations inside `cpu_step()` beyond executing the instruction:

| Field | Must be set to |
| --- | --- |
| `gb->cpu.last_opcode` | the raw first byte, before dispatch |
| `gb->cpu.last_opcode_len` | 1, 2 or 3 (the byte length of this instruction) |
| `gb->cpu.last_cycles` | the T-cycles this call consumed |
| `gb->cpu.instruction_count` | incremented by one per instruction |
| return value | the same number as `last_cycles` |

`gb_trace_record(gb, pc_of_instruction, cycles)` is called from `gb_step()` and reads those fields, so if they are stale or zero your `--trace` output is fiction. Call `cpu_fatal(gb, "illegal opcode %02X at %04X", op, pc)` for exactly these eleven opcodes:

```
D3 DB DD E3 E4 EB EC ED F4 FC FD
```

Everything else you have not implemented yet must reach `GB_UNIMPLEMENTED("opcode %02X at %04X", op, pc)` with a full register dump. Never NOP. Never "return 4 and continue". Never call `bus_tick()` from `cpu_step()`.

Instructions to implement in M02 (the rest of the table is a hole, by design):

| Op | Mnemonic | Bytes | T-cycles |
| --- | --- | --- | --- |
| `00` | `NOP` | 1 | 4 |
| `01 11 21 31` | `LD rr,d16` (`rr` = BC/DE/HL/SP) | 3 | 12 |
| `06 0E 16 1E 26 2E 3E` | `LD r,d8` | 2 | 8 |
| `36` | `LD (HL),d8` | 2 | 12 |
| `C3` | `JP a16` | 3 | 16 |
| `EA` | `LD (a16),A` | 3 | 16 |
| `FA` | `LD A,(a16)` | 3 | 16 |

Encoding, so you can see the pattern instead of transcribing 256 rows:

```
LD r,d8   :  0x06 + r*8      with r = 0..7 in B C D E H L (HL) A order
LD rr,d16 :  0x01 + rr*0x10  with rr = 0..3 in BC DE HL SP order
r8 index  :  0=B 1=C 2=D 3=E 4=H 5=L 6=(HL) 7=A      (index 6 costs +4 T and touches the bus)
```

`cpu_reset` (extend the provided skeleton if it does not already do this): zero `a..l`, `f = 0`, clear `ime`/`ime_pending`/`halted`/`halt_bug`/`stopped`, reset `instruction_count` and `last_*`. The M02 tests set `pc` and `sp` themselves, so a zeroed register file is fine here. The post-boot values (`pc = 0x0100`, `sp = 0xFFFE` and the rest of the register/IO table) arrive in M06 via `gb_apply_post_boot_state()`: do not put them in `cpu_reset`, and do not put them here.

## Work order

1. `cpu_reset` + the inline accessors you need. Confirm for yourself that `cpu_af()` masks `f & 0xF0`; if you ever store a flag in bit 3, that mask is why nothing reads it back.
2. `cpu_fetch8`/`cpu_fetch16`. Write the PC-timeline on paper for `LD BC,d16`: PC on entry, after the opcode fetch, after the low byte, after the high byte.
3. The dispatch table. Recommended shape (docs/02 section 2.7, option A): `{ mnemonic, bytes, cycles, cycles_taken, fn }`, one table for the base set and one for `0xCB` (empty until M04). Add the self-check that every one of the 256 base entries has a non-NULL `fn` or is one of the eleven illegal opcodes.
4. Implement `NOP`, `LD rr,d16`, `LD r,d8`, `LD (HL),d8`, `JP a16`, `LD (a16),A`, `LD A,(a16)`. Wire `cpu_read_r8`/`cpu_write_r8` for all eight indices now - index 6 is the only interesting one.
5. Fill `last_opcode`, `last_opcode_len`, `last_cycles`, `instruction_count` in `cpu_step`, and return the same cycle count. Run `.\gb\build.cmd -Test m02_step_cycles_nop`.
6. The two failure paths: `cpu_fatal()` for the eleven illegal opcodes, `GB_UNIMPLEMENTED()` for everything else. Run `m02_illegal_opcode_traps`.
7. PC semantics: `m02_nop_advances_pc`, `m02_ld_rr_d16`, `m02_ld_r_d8`, `m02_pc_after_2byte`.
8. Trace sanity: after `gb_step()`, dump the last entries with `gb_trace_dump(stdout)` in a throwaway test and confirm the recorded opcode bytes, PC and cycle count match what you executed. Delete the throwaway.
9. Run the full filter, then the whole suite: `.\gb\build.cmd -Test m02`, `.\gb\build.cmd -Test`. Your `m01_` tests must still be green and the unimplemented-opcode count must be visible as red tests, not aborts.
10. Commit with the cycle/flag decisions you made written into `NOTES.md`.

## Acceptance tests

```
.\gb\build.cmd -Test m02
.\gb\build.cmd -Test m02_step_cycles_nop   # one at a time while debugging
.\gb\build.cmd -Test                       # m01 green, everything else a red to-do list
```

Pass criterion: `m02_nop_advances_pc`, `m02_ld_rr_d16`, `m02_ld_r_d8`, `m02_pc_after_2byte`, `m02_step_cycles_nop`, `m02_illegal_opcode_traps` all PASS; `cpu_step()` returns 4 / 8 / 12 / 16 exactly as the table above; and executing `0xD3` sets `gb->fatal` with a printed dump while executing, say, `0x0A` produces an `UNIMPLEMENTED:` failure and leaves the process alive under `-Test`.

## Common traps

- Double clocking. Symptom: `gb.total_ticks` after one `gb_step()` equals twice the return value, or `DIV` runs at half speed. Cause: calling `bus_tick()` (or `timer_tick()`) from `cpu_step()` as well as from `gb_step()`. Detect: assert `gb_step(gb) == gb.total_ticks_delta` in a test; read `gb/src/gb.c` once and fix one source of truth.
- PC incremented twice for immediates. Symptom: `m02_pc_after_2byte` fails by exactly one, and later `JR` lands one byte short. Cause: `cpu_fetch16` already advances PC, then the handler does `pc += 2` again, or a handler computes the operand address as `pc - 1`. Detect: after `LD HL,d16` from `0x0100`, PC must be `0x0103`.
- Little-endian read backwards. Symptom: every 16-bit immediate is byte-swapped and every 16-bit ROM address is wrong; `LD HL,d16` with the bytes `34 12` loads `0x3412` instead of `0x1234`. Cause: `cpu_fetch16` reading the high byte first, or writing `lo << 8 | hi`. Detect: `m02_ld_rr_d16` with a value whose two bytes differ (`0x1234`, never `0x1111`).
- Illegal opcode sent down the wrong path. Symptom A: `m02_illegal_opcode_traps` fails because `gb->fatal` is never set. Symptom B: the whole suite dies on the first unimplemented instruction because you called `cpu_fatal()` for everything not in your table. Cause: conflating "hardware cannot reach this" (`cpu_fatal`) with "you have not written it yet" (`GB_UNIMPLEMENTED`). Detect: `0xD3` must set `fatal`; `0x0A` must print `UNIMPLEMENTED:` and let the runner continue.
- `last_opcode` never set. Symptom: `--trace` shows zeroes or the bytes of the *previous* instruction; a crash report points at the wrong instruction. Cause: setting the trace fields after dispatch, or only on some paths. Detect: dump the trace after one `gb_step()` and compare with the bytes you know you placed.
- Timer expressed in M-cycles. Symptom: every cycle test fails by exactly 4x, and `m02_step_cycles_nop` expects 4 but gets 1. Cause: copying the M-cycle column of an opcode table. Detect: the ratio is exactly 4 on every entry; multiply the whole table and re-run.
- Table holes that silently dispatch to the previous case. Symptom: an unimplemented opcode "works" but does the wrong thing; a `switch` without `default:` falls through. Cause: a missing `break` or a table entry pointing at the wrong handler. Detect: assert the table is complete (every entry non-NULL or explicitly illegal) and that an unknown opcode reaches `GB_UNIMPLEMENTED`.
- `r8` indices shifted. Symptom: `LD H,d8` writes to `L`, or `(HL)` reads from HL but `LD (HL),d8` writes to `(HL+1)`. Cause: index 6 placed at position 7, or `A` at 6. Detect: a table-driven test over all eight indices from `cpu.h`, not a spot check.

## Hint ladder

### H1

- Who owns the clock: `cpu_step` or `gb_step`? What exactly does `gb_step` do with the number you return, and what would happen if both of you did it?
- When `cpu_step` is entered, what does `pc` point at, and what does it point at when `cpu_step` returns? Write both answers for a 1-byte, a 2-byte and a 3-byte instruction.
- Which of these is a guest fault and which is your own unimplemented code: `0xD3`, `0x0A`, `0xCB`, `0x76`? Where does each one go, and who is allowed to abort?
- What does `gb_trace_record()` read, and in what order must you set those fields relative to executing the instruction?

### H2

- Technique: derive the two encoding patterns once, on paper: `dst = (op >> 3) & 7` and `src = op & 7` for the load block, `op = base + operand` for immediates. M03 then becomes two loops of eight, and M04 two tables of 256 that you generate, not type.
- Technique for tests: create a `gb_t`, put the program bytes into the cartridge image you built in M01 (`cart.rom` or WRAM through `bus_write`), set `pc`, call `cpu_step(gb)` once, and assert registers plus the *return value* separately. Asserting only registers misses cycle bugs; asserting only cycles misses operand bugs.
- Technique: implement `GB_UNIMPLEMENTED` coverage first and the instructions second. A table where every hole is loud turns M03/M04 into a progress bar of failing names instead of a hang.
- Spec pointers: docs/02 sections 2.3 (the three cycle models, and why you start with the instruction total), 2.5 (fetch and PC), 2.7 (table shapes); `reference/cheatsheet-opcode-map.md` for T-cycles and the illegal list.

### H3

- T-cycles for M02: `NOP` 4; `LD r,d8` 8; `LD (HL),d8` 12; `LD rr,d16` 12; `JP a16` 16; `LD (a16),A` 16; `LD A,(a16)` 16. All later totals come from the same table in the cheatsheet; the only values not in it are the conditional "taken" costs (M04).
- `fetch16`: `lo = fetch8(gb); hi = fetch8(gb); return (u16)(lo | (hi << 8));`. There is no other valid order; the CPU is little endian everywhere.
- `cpu_read_r8` index order is `B C D E H L (HL) A`; index 6 must call `bus_read(gb, cpu_hl(&gb->cpu))` and `cpu_write_r8` index 6 must call `bus_write(gb, cpu_hl(&gb->cpu), value)`. Every other index is a plain field.
- Illegal list, exactly eleven: `D3 DB DD E3 E4 EB EC ED F4 FC FD`. Real hardware stops fetching and hangs; in your emulator that is `cpu_fatal()`, not `GB_UNIMPLEMENTED()`.

## Done when

- `.\gb\build.cmd -Test m02` reports 6/6 PASS and `m01_` is still green.
- `cpu_step()` returns 4/8/12/16 for the seven M02 instructions, verified by tests, not by reading the source.
- `cpu_step()` never calls `bus_tick`/`timer_tick`/`ppu_tick`; `gb_step()` is the only clock forwarder (grep the call sites to prove it).
- `--trace` output for a short hand-built program shows the correct raw bytes, PC and T-cycles for every executed instruction.
- Executing `0xD3` sets `gb->fatal`; executing any unwritten legal opcode fails exactly one test with `UNIMPLEMENTED: <file>:<line>`.
- `NOTES.md` records the dispatch-table shape you chose, the `last_*` field ordering, and the list of opcodes still to come in M03/M04.

## Stretch

- Fill the `cycles_taken` column now (`JR cc`, `JP cc`, `CALL cc`, `RET cc`) even though M04 implements them; it is mechanical and it prevents two conflicting sources of truth.
- Make `gb_disasm()` (M11's job) unnecessary by printing the mnemonic from your table in `cpu_fatal()`'s dump. It costs three lines and makes every future crash report readable.
- Add a random-opcode smoke test: feed 4096 random bytes through `cpu_step` and assert the process only ever reports `UNIMPLEMENTED` or `fatal`, never crashes. This is the cheap version of M12's differential testing.

## Commit

```
feat(cpu): register file, fetch/decode table, NOP/LD/JP; loud abort on holes and illegal opcodes (M02 green)
```
