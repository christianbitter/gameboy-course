# M03 - 8-bit core

**Goal** - Implement the 64-entry register-to-register load matrix, the entire 8-bit ALU block and its `d8` forms, `INC`/`DEC` on registers and `(HL)`, and the accumulator-only rotates plus `CPL/SCF/CCF/DAA` - with the H and C flags exactly right.

**Estimated effort** - 8-12 h over 2-3 sessions. Budget more for the flag tests than for the instructions; the instructions are a table, the flags are where the days go.

## Read first

- `docs/02-cpu.md` section 2.4 (the full flag rules table) and section 2.5 subsection "The `(HL)` operand". Section 2.4 is the specification for this milestone; everything below is the workflow around it.
- `reference/cheatsheet-opcode-map.md` - T-cycles per group, and the "flag effects at a glance" table. Note the note at the top of `docs/02-cpu.md` section 2.1 against it if the two ever disagree.
- `gb/include/gb/cpu.h` - `cpu_read_r8`/`cpu_write_r8` (index 6 is `(HL)`), the `CPU_FLAG*` macros.
- `gb/include/gb/common.h` - `GB_FLAG_Z 0x80`, `GB_FLAG_N 0x40`, `GB_FLAG_H 0x20`, `GB_FLAG_C 0x10`. `common.h` is authoritative for the bit positions.
- `gb/tests/t_m03_alu.c` - read the data tables before you write the implementations. The tests are the specification of the corner cases; guessing them from the spec is slower.
- Pan Docs "CPU Registers and Flags" and the opcode tables at <https://gbdev.io/gb-opcodes/optables/octal> - the octal view makes the `0x40-0x7F` and `0x80-0xBF` patterns visible.

## Why this milestone exists

This is the block where a plausible-looking implementation passes half the tests. The load matrix and the logic ops are easy; `ADC`, `SBC`, `CP`, `INC`/`DEC` and `DAA` are where the half-carry and the borrow rules bite, and every one of those bugs produces *no symptom at all* until a game's decimal score display or a `cp`-based state machine misbehaves three milestones later. So M03 is deliberately shaped as ten data-driven tests over hundreds of vectors: you are not testing "does ADD work", you are testing every flag cell against a table. It is also the first milestone where the `(HL)` operand appears, and getting its extra 4 T-cycles and its bus access right now is what makes M09's access-level timing a refinement instead of a rewrite. Do the ALU as two loops of eight over `cpu_read_r8`; 128 hand-written cases means 128 chances to typo the flag logic.

## Deliverable contract

Extend the dispatch table and the `0xCB`-free part of `gb/src/opcodes.c`. All of the following must dispatch, with correct results, flags and T-cycles:

| Range / op | What | T-cycles |
| --- | --- | --- |
| `0x40-0x7F` | `LD dst,src`, `dst = (op>>3)&7`, `src = op&7` | 4, or 8 if `dst` or `src` is `(HL)` |
| `0x76` | `HALT` - **not** this milestone | - |
| `0x80-0xBF` | `op A,r`, `op = (op-0x80)>>3`, `r = op&7`; order ADD ADC SUB SBC AND XOR OR CP | 4, or 8 if `r` is `(HL)` |
| `0xC6 CE D6 DE E6 EE F6 FE` | the same eight ops with `d8` | 8 |
| `0x04 0C 14 1C 24 2C 3C` and `0x05 0D 15 1D 25 2D 3D` | `INC r` / `DEC r` | 4 |
| `0x34` / `0x35` | `INC (HL)` / `DEC (HL)` | 12 |
| `0x07 0F 17 1F 2F 37 3F 27` | `RLCA RRCA RLA RRA CPL SCF CCF DAA` | 4 |

Signatures: none new. You use the M02 table, `cpu_read_r8`/`cpu_write_r8` (index 6 = `(HL)`), and the `CPU_SET_FLAG`/`CPU_CLR_FLAG` macros. `cpu_step()`'s `last_opcode*`/`last_cycles`/`instruction_count` obligations from M02 continue to apply to every new entry.

The ALU block is one implementation, not eight: a single helper with an explicit carry-in and an explicit "write A or not" flag is the only shape that keeps `CP` honest.

Worked vectors the tests will hit (derive the rest from docs/02 section 2.4 - do not memorise these, use them to calibrate):

| Case | Result | F afterwards |
| --- | --- | --- |
| `ADD A,B` with A=`0x3A`, B=`0xC6` | `0x00` | Z=1 N=0 H=1 C=1 (`0xF0`) |
| `ADD A,B` with A=`0x0F`, B=`0x01` | `0x10` | Z=0 N=0 H=1 C=0 (`0x20`) |
| `ADC A,B` with A=`0x0F`, B=`0x00`, carry=1 | `0x10` | Z=0 N=0 H=1 C=0 (`0x20`) |
| `SUB B` with A=`0x3E`, B=`0x0F` | `0x2F` | Z=0 N=1 H=1 C=0 (`0x60`) |
| `SBC A,B` with A=`0x3E`, B=`0x0F`, carry=1 | `0x2E` | Z=0 N=1 H=1 C=0 (`0x60`) |
| `CP B` with A=`0x00`, B=`0x01` | A unchanged | Z=0 N=1 H=1 C=1 (`0x70`) |
| `AND B` with A=`0xF0`, B=`0x0F` | `0x00` | Z=1 N=0 H=1 C=0 (`0xA0`) |
| `OR B` / `XOR B` with a zero result | `0x00` | Z=1 N=0 H=0 C=0 (`0x80`) |
| `INC A` with A=`0xFF` | `0x00` | Z=1 N=0 H=1, **C unchanged** |
| `DEC A` with A=`0x00` | `0xFF` | Z=0 N=1 H=0, **C unchanged** |

## Work order

1. Write the incomplete `0x40-0x7F` handler as one loop: decode `dst` and `src`, call `cpu_read_r8` then `cpu_write_r8`, add the `(HL)` penalty, and leave `0x76` as an explicit `GB_UNIMPLEMENTED("HALT (M05)")`. Run `.\gb\build.cmd -Test m03_ld_r_r_matrix`.
2. `m03_hl_indirect_cycles`: assert the four shapes - `LD r,(HL)` 8, `LD (HL),r` 8, `LD r,r'` 4, `LD (HL),d8` 12. Fix the cycle arithmetic before adding instructions.
3. The eight ALU ops against a register operand (`0x80-0xBF`) via the same loop, using one shared ALU helper. Run `m03_alu_add_flags`, `m03_alu_sub_flags`, `m03_alu_logic_flags`.
4. Carry-in forms: `ADC`/`SBC` must consume the incoming C flag in *both* the result and the H computation. Run `m03_alu_adc_flags` / `m03_alu_sbc_flags`.
5. `CP`: flags of `SUB`, but A is not written. Run `m03_alu_cp_no_write`. If this passes while `SUB` passes, your helper's "write back" decision is explicit, which is what you want.
6. The `d8` forms (`0xC6`..`0xFE`): fetch one byte, then the same helpers. 8 T-cycles each.
7. `INC`/`DEC` on registers, then on `(HL)` at 12 T-cycles. C must be untouched by both. Run `m03_inc_dec_flags`.
8. `RLCA/RRCA/RLA/RRA`: Z is **forced to 0** (unlike the `CB` versions in M04), N and H are cleared, C is the bit rotated out. Run `m03_rotates_a`.
9. `CPL` (N=1, H=1), `SCF` (C=1, N=H=0), `CCF` (C inverted, N=H=0). Assert the flags you did *not* intend to touch.
10. `DAA` against the five vectors in docs/02 section 2.4. Run `m03_daa`. DAA reads N, H and C and writes Z, H, C; get the correction digits and the order of the two tests from the doc.
11. Full filter plus regression: `.\gb\build.cmd -Test m03`, then `.\gb\build.cmd -Test m0` to confirm M01/M02 did not regress.

## Acceptance tests

```
.\gb\build.cmd -Test m03
.\gb\build.cmd -Test m03_daa        # the one most worth iterating on alone
.\gb\build.cmd -Test m0             # M00-M06 prefix: regression sweep
```

Pass criterion: `m03_ld_r_r_matrix`, `m03_alu_add_flags`, `m03_alu_sub_flags`, `m03_alu_adc_flags` / `m03_alu_sbc_flags`, `m03_alu_logic_flags`, `m03_alu_cp_no_write`, `m03_inc_dec_flags`, `m03_daa`, `m03_rotates_a`, `m03_hl_indirect_cycles` all PASS, with the tables exercising every `r8` index, both `(HL)` positions, and the flag cells listed in docs/02 section 2.4.

## Common traps

- `AND` sets H = 1. Symptom: only `m03_alu_logic_flags` fails, on the H bit. Cause: "logic ops clear N, H, C" is true for XOR and OR and false for AND; the half-carry exists for BCD correction. Detect: assert `F == 0xA0` for `AND` producing zero, `0x80` for `OR`/`XOR` producing zero.
- `CP` writing A. Symptom: `m03_alu_cp_no_write` fails; games that compare-then-use-A branch wrongly. Cause: reusing the `SUB` code path including its store, or storing the truncated result before the flags are computed. Detect: `assert(a_before == a_after)` in the test, plus a flag-only assertion.
- Half-borrow computed as a carry. Symptom: `SUB`/`SBC`/`CP` fail on H only. Cause: `(a & 0xF) + (n & 0xF) > 0xF` instead of `(a & 0xF) < (n & 0xF)`. Detect: `SUB B` with A=`0x00`, B=`0x01` must set H (borrow out of bit 3) and C, not clear them.
- `ADC`/`SBC` ignoring the carry in the H/C rule. Symptom: `m03_alu_adc_flags` / `m03_alu_sbc_flags` fails only for `carry_in == 1` vectors. Cause: result computed with the carry but flags computed without, or vice versa. Detect: the test's carry-in variants; make sure both operands of the H rule include the carry.
- `INC`/`DEC` clobbering C. Symptom: a loop that uses `INC` inside a `cp`-carry chain hangs or exits late. Cause: calling the shared ALU helper which necessarily writes C. Detect: `m03_inc_dec_flags` asserts C unchanged for all eight registers.
- `DAA` corrections and their order. Symptom A: `0x9A -> 0x06` instead of `0x00` (transposed corrections). Symptom B: corrections are right but `0x9A` still fails. Cause A: using `0x06` for the high nibble; the high correction is `0x60` and the low is `0x06`. Cause B: testing `(A & 0x0F) > 0x09` against the original A instead of A **after** the high-nibble correction - the second test must see the corrected value. Detect: the five vectors in docs/02 section 2.4, in particular `0x9A -> 0x00`, `0xFF` (N=1, both corrections) and `0x2D` (N=1, H=1).
- Flag bit positions swapped. Symptom: every ALU test fails on H and C in a mirrored way (`H` set where `C` should be). Cause: the name row of the flag table in `docs/02-cpu.md` section 2.1 labels bit 5 as `C`; the values and `gb/include/gb/common.h` are correct: **H = `0x20`, C = `0x10`**. Detect: print `F` as two hex digits and compare against the expected value in the test table; a mirror image means you used the doc's name row.
- `(HL)` operand cost applied twice. Symptom: `m03_hl_indirect_cycles` fails at 12 T where 8 is expected, and any ROM with `(HL)` loads runs slow. Cause: adding the +4 in the handler *and* in the table, or counting `dst == 6 || src == 6` for both halves of a `LD (HL),(HL)`-shaped check. Detect: the four-shape test in step 2; one `(HL)` operand is +4, two would be +8 and cannot occur (`0x76` is `HALT`).

## Hint ladder

### H1

- Which of the eight ALU ops sets H unconditionally to 1, and what is that flag actually for?
- For `ADC`, name the three inputs to the flag computation and the three inputs to the result computation. Are they the same set?
- Why does `INC (HL)` cost 12 T-cycles when `INC A` costs 4? What extra work exists, and how does that work reach the cartridge/WRAM?
- `RLCA` and `RLA` both rotate left; what is the difference in where the new bit 0 comes from, and what happens to Z in both?

### H2

- Technique: one ALU helper, `(u8 a, u8 operand, bool carry_in) -> (result, new_flags)` with an explicit "clobber A" parameter for `CP`. Flags are computed in `int`, then truncated to `u8`; `docs/02-cpu.md` section 2.4 "Arithmetic implementation notes" shows the shape.
- Technique: the tests are data-driven; when one row fails, print the row index and the full `A F` pair. Debugging 200 vectors by breakpoint is not a plan. Extend the table with the case that broke instead of adding a `printf` in the emulator.
- Technique for the two loops: build the dispatch entries so that `dst`/`src`/`op`/`r` come from the low three bits of the opcode, then call the same handler for the `r` and `d8` forms with a different operand source. That is roughly 40 lines instead of 128 cases.
- Spec pointers: `reference/cheatsheet-opcode-map.md` "Flag effects at a glance", docs/02 section 2.4 (with the `DAA` worked checks), docs/02 section 2.8 (the "my CPU tests fail" checklist - read it *before* debugging).

### H3

- Flag rules you will need most: `Z = (result == 0)`; `ADD/ADC`: `N=0`, `H = ((a & 0xF) + (n & 0xF) + carry) > 0xF`, `C = a + n + carry > 0xFF`; `SUB/SBC/CP`: `N=1`, `H = (a & 0xF) < ((n & 0xF) + carry)`, `C = a < (n + carry)`; `AND`: `H=1, C=0`; `OR/XOR`: `H=0, C=0`; `INC`: `H = ((r & 0xF) == 0xF)`, C unchanged; `DEC`: `H = ((r & 0xF) == 0x0)`, C unchanged.
- `DAA`, in this exact order: if `N == 0`, then `if (C || A > 0x99) { A += 0x60; C = 1; }` and **then** `if (H || (A & 0x0F) > 0x09) A += 0x06;`. If `N == 1`, then `if (C) A -= 0x60;` and `if (H) A -= 0x06;`. Afterwards `A &= 0xFF`, `Z = (A == 0)`, `H = 0`, N unchanged, C as set.
- Rotates: `RLCA`: `C = A >> 7`, `A = (A << 1) | C`; `RRCA`: `C = A & 1`, `A = (A >> 1) | (C << 7)`; `RLA`: `t = A >> 7`, `A = (A << 1) | (old C)`, `C = t`; `RRA`: `t = A & 1`, `A = (A >> 1) | (old C << 7)`, `C = t`. All four force `Z=0, N=0, H=0`.
- The `(HL)` rule: index 6 in `cpu_read_r8`/`cpu_write_r8` is a bus access at `HL` and costs 4 T-cycles on top of the base cost of the operation. `INC (HL)` and `DEC (HL)` are 12 because they are a read *and* a write.

## Done when

- `.\gb\build.cmd -Test m03` reports 10/10 PASS, and `m0` (M00-M06 prefix) shows no regression in M01/M02.
- The load matrix passes for all 64 opcodes including both `(HL)` positions, with 4 T for register-to-register, 8 T when one side is `(HL)`, 12 T for `LD (HL),d8`.
- `CP` leaves A untouched while producing `SUB`'s flags, proven by a test assertion, not by reading the code.
- `INC`/`DEC` leave C unchanged for all eight `r8` indices, proven by a test.
- `DAA` passes all five worked vectors in docs/02 section 2.4, including the subtraction pair.
- `F` is only ever read back with the low nibble masked or zero; no test ever sees a bit outside `0xF0`.

## Stretch

- Replace the flags-in-a-byte implementation with a cached `(bool z, n, h, c)` representation behind the same accessors, and re-run all ten tests unchanged. If the tests cannot tell, your flag API is right; if they can, you learned something about F's masking.
- Write a Python reference for the eight ALU ops (20 lines with Python ints, which cannot overflow), generate 4096 random `(a, operand, carry)` triples, and diff against your C helper by dumping C's results to a file. This is the M12 technique, available a milestone early, and it will find the flag cell you mis-derived.
- Add the DB (decimal arithmetic) tests blargg's `cpu_instrs` uses: run `0x99 + 0x01` style BCD sequences through `ADD` then `DAA` and check the displayed digits, not just the flags.

## Commit

```
feat(cpu): 0x40-0x7F load matrix, 8-bit ALU + d8 forms, INC/DEC, rotates, DAA (M03 green)
```
