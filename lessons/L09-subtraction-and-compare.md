# L09 — Subtraction and compare

**Time** — theory ~25 min | coding ~55 min | verify ~10 min
**You will end with** — the borrow family: `SUB`, `SBC`, `CP`, and the `N` flag that
tells the rest of the CPU "the last operation was a subtraction".
**Tests that must go green** — `m03_alu_sub_flags`, `m03_alu_sbc_flags`,
`m03_alu_cp_no_write`
**Depends on** — L08 (the ALU block skeleton and the addition helper). You will edit
`gb/src/opcodes.c`.

---

## 1. Read this — the whole theory for today

### The same block, three more operations

You already have the decode from L08: `operation = (op - 0x80) >> 3`, operand
`= op & 7`. Today:

| operation | instruction | immediate form | Bytes | T-cycles |
| --- | --- | --- | --- | --- |
| 2 | `SUB r` | `0xD6 SUB d8` | 1 / 2 | 4 (8 for `(HL)`) / 8 |
| 3 | `SBC A,r` | `0xDE SBC A,d8` | 1 / 2 | 4 (8) / 8 |
| 7 | `CP r` | `0xFE CP d8` | 1 / 2 | 4 (8) / 8 |

`CP` is `SUB` **without writing the result to A**. Everything else about it — the
flags, the operand, the timing — is identical. It exists so a program can compare
without destroying a value.

### The borrow flags

`SUB r`:

```
res = (A - n) & 0xFF
Z   = (res == 0)
N   = 1
H   = ((A & 0xF) - (n & 0xF)) < 0          /* borrow out of bit 4 */
C   = A < n                                /* borrow out of bit 8  */
```

`SBC A,r` adds `carry` to the same three places:

```
res = (A - n - carry) & 0xFF
Z   = (res == 0)
N   = 1
H   = ((A & 0xF) - (n & 0xF) - carry) < 0
C   = (A - n - carry) < 0
```

Two things to notice, because they are where the hours go:

* **`H` and `C` are borrow tests, not carry tests.** Compute in `int` and test
  `> 0xFF` and you will get a *different* answer on most vectors — it looks right on
  the simple cases and fails exactly where a borrow happens.
* **`SBC`'s carry-in appears in both tests.** Reading the incoming `C` after you have
  already cleared `F` is the other classic bug; read it first, into a local.

### `CP` must not write A

`m03_alu_cp_no_write` asserts the destination is unchanged, and the fixture's
`want_a` equals the *input* `A`. So:

```
if (op is CP) { compute flags only }
```

The simplest shape that never goes wrong: one helper that returns the result and sets
the flags, plus a boolean "store the result" parameter — `false` for `CP`.

### Worked examples — the test vectors

| Instruction | A | operand | carry in | result | F | why |
| --- | --- | --- | --- | --- | --- | --- |
| `SUB B` | `0x10` | `0x01` | — | `0x0F` | `0x60` | `0 - 1` borrows -> H; `0x10 >= 0x01` -> no C |
| `SUB B` | `0x00` | `0x01` | — | `0xFF` | `0x70` | borrow -> H and C |
| `SUB B` | `0x05` | `0x05` | — | `0x00` | `0xC0` | Z and N only |
| `SUB B` | `0x88` | `0x11` | — | `0x77` | `0x40` | `8 - 1 = 7`, no borrow |
| `SBC A,B` | `0x10` | `0x01` | 1 | `0x0E` | `0x60` | `0 - 1 - 1` borrows -> H; `0x10 >= 2` -> no C |
| `SBC A,B` | `0x00` | `0x00` | 1 | `0xFF` | `0x70` | `0 - 0 - 1` borrows -> H and C |
| `SBC A,B` | `0x05` | `0x02` | 0 | `0x03` | `0x40` | N only |
| `CP B` | `0x10` | `0x01` | — | `0x10` (unchanged) | `0x60` | like `SUB`, A untouched |
| `CP B` | `0x05` | `0x05` | — | `0x05` | `0xC0` | equal -> Z N |
| `CP B` | `0x00` | `0x01` | — | `0x00` | `0x70` | borrows -> N H C |

`SBC A,d8` and `CP d8` are the same rules with the operand fetched instead of read
from a register. The immediate forms are still exactly the `(op & 0xC7) == 0xC6`
family: `0xD6`, `0xDE`, `0xFE`.

> **What matters for the code you are about to write**
>
> * Read the incoming carry **before** touching `gb->cpu.f`: `int carry = (gb->cpu.f &
>   GB_FLAG_C) ? 1 : 0;`
> * `H` is `((A & 0xF) - (n & 0xF) - carry) < 0`, `C` is `(A - n - carry) < 0`. Both
>   are borrows computed in `int`. `N` is set. `Z` is `res == 0`.
> * `CP` computes the flags and does **not** write `A`. If you reuse the `SUB` path,
>   make the store conditional, or the test's `want_a` will catch you.
> * The `(HL)` operand still costs +4 T-cycles (`SUB (HL)` is 8), and `SBC`/`CP` follow
>   the same rule.
> * Build the flag byte from zero on every operation so a stale `H` or `C` can never
>   survive into a result.

---

## 2. Your task — 55 min

Work in `gb/src/opcodes.c`.

1. **The subtraction helper** (25 min). One function that takes the operand and the
   carry-in, computes `res`, `Z`, `N`, `H`, `C`, writes `A` when asked to, and returns
   the T-cycles. Write the flag rules exactly as they appear above — no shortcuts.
2. **Wire the six forms** (15 min): `0x90-0x97` (`SUB r`), `0x98-0x9F` (`SBC A,r`),
   `0xB8-0xBF` (`CP r`), `0xD6`, `0xDE`, `0xFE`.
3. **The `(HL)` variant** (5 min). Make sure `SUB (HL)`/`SBC (HL)`/`CP (HL)` read
   through `cpu_read_r8(gb, 6)` and return 8 T, not 4.
4. **Run the tests** (5 min):
   ```
   .\gb\build.cmd -Test m03_alu
   ```
   `m03_alu_sub_flags`, `m03_alu_sbc_flags`, `m03_alu_cp_no_write` green.
   `m03_alu_logic_flags`, `m03_inc_dec_flags`, `m03_rotates_a`, `m03_daa` still red —
   L10-L11.
5. **Break it on purpose** (5 min). In a scratch copy, change `H` to use `> 0xF`
   (carry semantics) and watch which vectors fail. Two minutes of that is worth more
   than an hour of reading, because it shows you the *shape* of a borrow bug: the
   simple vectors still pass.

---

## 3. Prove it — 10 min

```
.\gb\build.cmd -Test m03_alu
```
```
[       OK ] m03_alu_cp_no_write
[       OK ] m03_alu_sbc_flags
[       OK ] m03_alu_sub_flags
[  FAILED ] m03_alu_logic_flags      <- L10
...
```
Then check nothing regressed:
```
.\gb\build.cmd -Test m0
```
`m00`-`m03` should be no worse than at the end of L08, and the only new failures should
be features you have not written.

---

## 4. If it fails

| Symptom | Cause | Find it |
| --- | --- | --- |
| Every "borrow" vector fails, simple ones pass | you test `H`/`C` as carries (`> 0xF`, `> 0xFF`) instead of borrows | the formulas above; compute in `int` |
| Only `SBC` fails | the carry-in reaches the result but not the `H`/`C` tests (or the reverse) | all three places |
| `m03_alu_cp_no_write` fails with `A=0x0F` | `CP` wrote the result into `A` | make the store conditional |
| `N` is clear after `SUB` | you copied the `ADD` helper's `N = 0` | `N = 1` for the whole borrow family |
| `SBC` with `carry = 0` passes, `carry = 1` fails | you read the carry *after* clearing `F` | read it into a local first |
| `SUB (HL)` returns 4 | the `(HL)` penalty is missing on the new path | `op & 7 == 6` -> 8 T |

---

## 5. Done when

- [ ] All three tests for this lesson are green
- [ ] `-Test m0` shows no regression
- [ ] You can state the `H` rule for `SUB` and for `SBC` without looking
- [ ] You can explain in one sentence why `CP` exists in the instruction set
- [ ] The deliberate `> 0xF` experiment failed on exactly the vectors you predicted
- [ ] Commit message like `feat(alu): SUB, SBC, CP with borrow flags (L09)`

---

## 6. Optional, only if you have time

* `docs/02-cpu.md` §2.4 — the complete family table, including `DAA` (L11) and the
  16-bit `ADD HL,rr` rules (L13).
* Implement `0xB7 OR A` etc. for the `(HL)`-free logic ops now if you want a head
  start; they are L10's.

Next: **[L10 — Logic, counting, and the (HL) cost](L10-logic-counting-and-the-hl-cost.md)**
