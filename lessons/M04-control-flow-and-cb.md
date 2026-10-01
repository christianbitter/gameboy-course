# M04 - Control flow, stack and the CB block

**Goal** - Make the machine programmable: conditional jumps and calls, returns, `RST`, pushes and pops, the 16-bit arithmetic and stack-pointer instructions, and all 256 `0xCB` shift/rotate/bit operations.

**Estimated effort** - 8-12 h over 2-3 sessions (jumps/calls ~3 h, stack-order bugs ~2 h, the `CB` block ~3 h, integration test ~2 h).

## Read first

- `docs/02-cpu.md` sections 2.6 (interrupt dispatch - read it now, implement in M05) and 2.5 (`PC` semantics, `LD (a16),SP`, the `(HL+)` ordering note).
- `docs/01-orientation.md` section 1.4 - determinism, because this is the milestone where a stack bug becomes a reproducible trace instead of a mystery.
- `gb/include/gb/cpu.h` - `cpu_af`/`cpu_set_af` and friends. Note that `cpu_set_af` masks `f & 0xF0`; that mask is the `POP AF` rule, already implemented for you.
- `reference/cheatsheet-opcode-map.md` - the `0xC0-0xFF` table with both `taken/not taken` costs, the condition-code table, the `RST` vector table and the `CB` block table.
- `gb/tests/t_m04_control.c` - in particular `m04_program_loop_sum`, which is the first test that runs a whole program.
- Pan Docs "CPU Instruction Set" (the `0xCB` prefix section) and <https://gbdev.io/gb-opcodes/optables/> for the `CB` sub-opcode grouping.

## Why this milestone exists

After M04 the CPU is complete except for interrupts, and a real ROM's control flow will run. The two things that break programs here are not conceptual: the `JR` offset base (relative to the byte *after* the instruction) and the stack byte order. Both produce output that looks like a random crash, and both are trivially testable, which is exactly why the milestone has dedicated tests for them. The `CB` block is volume, not difficulty: 256 sub-opcodes with a completely regular encoding, so if you write 256 cases you have chosen the hard way. It is also where `BIT`'s weird flag behaviour (H = 1, C unchanged) and the difference between the accumulator rotates in M03 (Z forced to 0) and the `CB` rotates (Z computed) get nailed down. And `m04_program_loop_sum` is your first end-to-end test: it runs a program built out of instructions from M02, M03 and M04, so a single wrong cycle count or PC update fails it in a way a unit test cannot.

## Deliverable contract

Add to the dispatch table (no new signatures; continue filling `cpu_step`'s `last_opcode*`, `last_cycles`, `instruction_count` obligations):

| Op | Mnemonic | T-cycles |
| --- | --- | --- |
| `C3` / `C2 CA D2 DA` | `JP a16` / `JP cc,a16` | 16 / 16 taken, 12 not taken |
| `18` / `20 28 30 38` | `JR e8` / `JR cc,e8` | 12 / 12 taken, 8 not taken |
| `E9` | `JP HL` | 4 |
| `CD` / `C4 CC D4 DC` | `CALL a16` / `CALL cc,a16` | 24 / 24 taken, 12 not taken |
| `C9 D9` / `C0 C8 D0 D8` | `RET` / `RETI` / `RET cc` | 16 / 16 / 20 taken, 8 not taken |
| `C7 CF D7 DF E7 EF F7 FF` | `RST tgt3` | 16 |
| `C5 D5 E5 F5` / `C1 D1 E1 F1` | `PUSH r16stk` / `POP r16stk` | 16 / 12 |
| `08` | `LD (a16),SP` | 20 |
| `E8` / `F8` / `F9` | `ADD SP,e8` / `LD HL,SP+e8` / `LD SP,HL` | 16 / 12 / 8 |
| `03 13 23 33` / `0B 1B 2B 3B` | `INC rr` / `DEC rr` | 8 |
| `09 19 29 39` | `ADD HL,rr` | 8 |
| `02 12 22 32` | `LD (BC),A` / `LD (DE),A` / `LD (HL+),A` / `LD (HL-),A` | 8 |
| `0A 1A 2A 3A` | `LD A,(BC)` / `(DE)` / `(HL+)` / `(HL-)` | 8 |
| `E0 F0` | `LDH (a8),A` / `LDH A,(a8)` | 12 |
| `E2 F2` | `LD (C),A` / `LD A,(C)` | 8 |
| `10` | `STOP` (2 bytes; second byte ignored here) | 4 |

`0xF3` (`DI`) and `0xFB` (`EI`) are M05: leave them as `GB_UNIMPLEMENTED` until you implement `ime`/`ime_pending`.

The `CB` block, all 256 sub-opcodes in one handler keyed off the sub-opcode byte:

| Sub range | Operation | T-cycles |
| --- | --- | --- |
| `00-3F` | `ROT[type] r8[sub & 7]`, `type = (sub>>3)&7`: RLC RRC RL RR SLA SRA SWAP SRL | 8, or 16 for `(HL)` |
| `40-7F` | `BIT n,r8`, `n = (sub>>3)&7` | 8, or 12 for `BIT (HL)` |
| `80-BF` | `RES n,r8` | 8, or 16 for `(HL)` |
| `C0-FF` | `SET n,r8` | 8, or 16 for `(HL)` |

Decide once whether the `0xCB` prefix fetch is included in those totals (conventionally it is, and `docs/02-cpu.md` section 2.3 says so). Then never revisit it.

Stack contract, exact byte order (a single off-by-one here breaks every `CALL`):

```
PUSH rr:  SP = SP - 1;  write bus at SP: HIGH byte of rr
          SP = SP - 1;  write bus at SP: LOW  byte of rr
POP  rr:  lo = read bus at SP;  SP = SP + 1
          hi = read bus at SP;  SP = SP + 1
          rr = (hi << 8) | lo      /* AF goes through cpu_set_af: F &= 0xF0 */
```

The `m04_program_loop_sum` test pokes a short program into the machine, sets `PC` and runs it to a sentinel. It uses only instructions from M02-M04. You do not need to know what it computes to make it pass - but when it fails, dump the trace before touching it, because the failing instruction is almost always two instructions before the wrong result.

## Work order

1. `JP a16`, `JR e8`, and their four conditions. Get the `JR` base right on the first try by writing the PC arithmetic on paper. Run `.\gb\build.cmd -Test m04_jp_jr_conditions`.
2. `CALL`/`RET`/`RETI`/`RET cc`. Implement `PUSH PC` once (the interrupt path in M05 reuses it). Run `m04_call_ret_stack`.
3. `PUSH`/`POP` for all four `r16stk`, with the byte order above and `cpu_set_af` on `POP AF`. Run `m04_push_pop_order`.
4. `RST` vectors `tgt3 * 8` for all eight. Run `m04_rst_vectors`.
5. 16-bit arithmetic: `INC rr`, `DEC rr` (no flags at all) and `ADD HL,rr` (Z preserved, N=0, H from bit 11, C from bit 15). Verify Z untouched with a test that pre-sets Z=1.
6. `ADD SP,e8`, `LD HL,SP+e8`, `LD SP,HL`: flags `Z=0, N=0`, H and C from the low-byte addition with `e8` treated as unsigned; the result uses `e8` as signed.
7. The block-0 loads (`LD (BC),A`, `LD (HL+),A`, `LDH`, `LD (C),A`, `LD (a16),SP`) including the memory-then-increment order for `(HL+)`/`(HL-)`.
8. The `CB` rotate/shift group. One helper per of the eight types, driven by a table. Run `m04_cb_rotates`.
9. `BIT`/`RES`/`SET`, all 8 bit indices and all 8 operands, and `BIT (HL)` at 12 T. Run `m04_cb_bit_res_set`.
10. Integration: `m04_program_loop_sum`. When it fails, `gb_trace_dump(stdout)` and find the first instruction whose register effect differs from what you expect - not the one where the answer finally goes wrong.
11. `STOP` as a 2-byte instruction; `DI`/`EI` stay unimplemented until M05.
12. Full filter and regression: `.\gb\build.cmd -Test m04`, then `.\gb\build.cmd -Test m0`.

## Acceptance tests

```
.\gb\build.cmd -Test m04
.\gb\build.cmd -Test m04_program_loop_sum   # the integration test, while debugging
.\gb\build.cmd -Test m0
```

Pass criterion: `m04_jp_jr_conditions`, `m04_call_ret_stack`, `m04_push_pop_order`, `m04_rst_vectors`, `m04_cb_rotates`, `m04_cb_bit_res_set`, `m04_program_loop_sum` all PASS; every condition code is covered in both the taken and not-taken direction (check the cycle counts too, not only the branch target); and `m04_program_loop_sum` passes with the trace showing a plausible instruction sequence.

## Common traps

- `JR` offset based on the wrong address. Symptom: forward jumps land one or two bytes early, backward loops skip their last instruction, and the machine eventually executes data. Cause: adding `e8` to the opcode address instead of to the PC after fetching the operand. Detect: `JR +0` must continue at the *next* instruction; `JR -2` must loop to itself.
- Conditional instruction cost never varies. Symptom: `m04_jp_jr_conditions` passes on targets and fails on cycles; later, `instr_timing` fails everywhere. Cause: one `cycles` column instead of `cycles`/`cycles_taken`. Detect: assert both directions explicitly (`JR NZ` with Z=0 is 12, with Z=1 is 8).
- Stack byte order and `POP AF` masking. Symptom A: `RET` returns to an address with its bytes swapped, or `POP HL` gives `0x3412` where you pushed `0x1234`. Symptom B: a stray bit in F's low nibble leaks into a `cp` chain or `DAA` code. Cause A: PUSH writing low then high, or POP reading high then low. Cause B: assigning `f` directly instead of going through `cpu_set_af`. Detect: `m04_push_pop_order` (assert the exact two addresses written: `SP-1` gets the high byte), plus push `0xFFFF` and assert `F == 0xF0` after `POP AF`.
- `RET cc` popping when the condition is false. Symptom: a subroutine returns to garbage only sometimes; SP drifts by 2 per skip. Cause: popping the address before evaluating the condition. Detect: assert `SP` unchanged on the not-taken path.
- `ADD HL,rr` half-carry from bit 3. Symptom: `ADD HL` flag tests fail H only; a `DAA`-driven 16-bit routine misprints. Cause: reusing the 8-bit add helper for a 16-bit add. Detect: `HL = 0x0FFF`, `BC = 0x0001` must set H and clear C.
- `ADD SP,e8` treating `e8` as signed in the flag computation. Symptom: negative offsets clear the carry that a positive equivalent would set. Cause: sign-extending before the low-byte addition; the flags come from `(SP & 0xFF) + (u8)e8`, unsigned. Detect: test `e8 = 0x01` and `e8 = 0xFF` on the same `SP` and compare the C/H bits.
- `CB` cost and flag handling wrong. Symptom A: every `CB` operation is 4 T too slow, or `BIT (HL)` costs 16 instead of 12. Symptom B: `BIT` sets C, or `SET`/`RES` clear Z. Cause A: the dispatcher adds the prefix fetch *and* the sub-table already includes it, or `BIT` was grouped with the read-modify-write ops. Cause B: one "update the flags" helper shared by all `CB` subgroups. Detect: assert 8 (register) / 16 (`(HL)` read-modify-write) / 12 (`BIT (HL)`), and assert that `BIT` is `Z = !((r >> n) & 1), N=0, H=1`, **C unchanged**, while `SET`/`RES` change no flags at all.
- `(HL+)`/`(HL-)` incrementing before the access. Symptom: `LD (HL+),A` writes one byte too high; block-copy routines corrupt the first byte. Cause: updating HL first. Detect: assert the byte landed at the *old* HL and that HL changed by exactly 1.

## Hint ladder

### H1

- What is the `JR` offset relative to, and how do you prove it with a two-instruction experiment (`JR +0`)?
- Name the two addresses a `PUSH HL` writes to, and the byte stored at each. Then name the order a `POP HL` reads them. Why does one of those orders break `RET` but not `POP AF`?
- Which instructions in this milestone affect no flags at all, and which one preserves Z but writes H and C?
- How many distinct `CB` operation types are there, and how many bits of the sub-opcode select the operand? If you find yourself typing more than about 20 `CB` cases, what have you misunderstood?

### H2

- Technique: implement `PUSH PC` as one function (`stack_push16(gb, pc)`) and call it from `CALL`, `RST`, and later from interrupt dispatch. One byte order, one place to be wrong.
- Technique: generate the condition tests from a table `{ opcode, cc_index, get_flag }` and assert both taken and not-taken for all four conditions in one loop. Seven tests, four conditions, both directions: if one combination is untested, that is the one that will be wrong.
- Technique: for `m04_program_loop_sum`, work backwards from the trace. Find the first instruction whose effect is not what the *test program* says it should be; the wrong final sum is usually three instructions downstream of the real bug.
- Spec pointers: `reference/cheatsheet-opcode-map.md` (`0xC0-0xFF` table, condition table, `RST` table, `CB` block table); docs/02 section 2.5 for `PC` and `(HL+/-)` ordering; docs/02 section 2.7 for keeping the table complete.

### H3

- `JR e8`: read `offset = (s8)fetch8(gb)`, then `pc = (u16)(pc + offset)` where `pc` is already past the operand. `JP cc`/`CALL cc`/`RET cc` evaluate `Z` for `Z/NZ` and `C` for `C/NC` only - never N or H.
- `RST tgt3`: `tgt3` is the top three bits of the opcode (`(op >> 3) & 7`), and the target is `tgt3 * 8`. `C7 -> 0x0000` ... `FF -> 0x0038`.
- `ADD SP,e8` result: `sp = (u16)(sp + (s8)e8)`; flags: `Z=0, N=0, H = ((sp & 0xF) + (e8 & 0xF)) > 0xF`, `C = ((sp & 0xFF) + (u8)e8) > 0xFF`. `LD HL,SP+e8` is the same arithmetic with the result in HL.
- `ADD HL,rr`: `int r = hl + rr; Z unchanged; N=0; H = ((hl & 0xFFF) + (rr & 0xFFF)) > 0xFFF; C = r > 0xFFFF; hl = (u16)r`.
- `CB` rotate/shift definitions (`r` is the 8-bit operand): `RLC`: `C = r >> 7`, `r = (r << 1) | C`; `RRC`: `C = r & 1`, `r = (r >> 1) | (C << 7)`; `RL`: `t = r >> 7`, `r = (r << 1) | (C_in)`, `C = t`; `RR`: `t = r & 1`, `r = (r >> 1) | (C_in << 7)`, `C = t`; `SLA`: `C = r >> 7`, `r <<= 1`; `SRA`: `C = r & 1`, `r = (r >> 1) | (r & 0x80)`; `SRL`: `C = r & 1`, `r >>= 1`; `SWAP`: `r = (r << 4) | (r >> 4)`, `C = 0`. All eight: `N=0, H=0, Z = (r == 0)`.
- `BIT n,r` is `Z = !((r >> n) & 1)`, `N=0`, `H=1`, C unchanged; `RES` clears and `SET` sets bit `n` with **no** flag changes.

## Done when

- `.\gb\build.cmd -Test m04` reports 7/7 PASS, and `m0` shows M01-M03 still green.
- All four condition codes are exercised taken and not-taken with the correct cycle counts, asserted in a test rather than by reading the table.
- `PUSH`/`POP` round-trips `0x1234` through every `r16stk`, and `POP AF` masks F's low nibble.
- `m04_program_loop_sum` passes, and you can point at the trace entries that show the loop iterating more than once.
- All 256 `CB` sub-opcodes dispatch with the correct cycles (8/16/12), including `BIT (HL)` at 12.
- `NOTES.md` records the `CB` prefix cycle decision so M07-M09 cannot re-litigate it.

## Stretch

- Write the missing `CB`-block test yourself: 256 sub-opcodes, each applied to a preloaded `A`, with expected values generated by a Python script. Then hide the Python and keep the table - that is exactly how M12's differential tests will feel.
- Implement `LD (a16),SP` and `ADD SP,e8` in an access-ordered way (`bus_write16` for the SP store) so that M09's access-level timing is a relocation, not a rewrite.
- Add a tiny in-test assembler helper (a `emit(op, ...)` function building the program for `m04_program_loop_sum`-style tests). It makes every later CPU test a one-liner and costs 30 lines.

## Commit

```
feat(cpu): conditional jumps/calls, stack, RST, 16-bit ALU, full 0xCB block (M04 green)
```
