# L12 — Jumps and your first program

**Time** — theory ~20 min | coding ~50 min | verify ~20 min
**You will end with** — conditional control flow, and a real program that runs to a
correct answer. AHA #4 (a small one, and a good one): *my CPU executed a program and
computed 55.*
**Tests that must go green** — `m04_jp_jr_conditions`, `m04_program_loop_sum`
**Depends on** — L06 (`JP a16`), L08 (`ADD`), L10 (`DEC B`). You will edit
`gb/src/opcodes.c`.

---

## 1. Read this — the whole theory for today

### Two ways to jump

| Opcode | Instruction | Bytes | T-cycles |
| --- | --- | --- | --- |
| `0xC3` | `JP a16` | 3 | 16 (already done in L06) |
| `0xC2` `0xCA` `0xD2` `0xDA` | `JP NZ,a16` / `JP Z,a16` / `JP NC,a16` / `JP C,a16` | 3 | **16 taken / 12 not taken** |
| `0x18` | `JR e8` | 2 | 12 |
| `0x20` `0x28` `0x30` `0x38` | `JR NZ,e8` / `JR Z,e8` / `JR NC,e8` / `JR C,e8` | 2 | **12 taken / 8 not taken** |

`JP` carries a full 16-bit address and can reach anywhere. `JR` carries a signed 8-bit
**offset** and saves a byte, which is why compilers of the era preferred it for loops.

### The offset is relative to the END of the instruction

This is the one thing to get right:

```
0x0106: 20 FC      JR NZ,-4
```

After the opcode and the offset byte are fetched, `PC` is already `0x0108`. The target
is `0x0108 + (-4) = 0x0104`. In other words:

```c
u16 target = gb->cpu.pc + (int8_t)offset;   /* pc is ALREADY past the instruction */
```

`s8` is `int8_t` and it is provided in `common.h`. `0xFE` is `-2`, `0xFC` is `-4`,
`0x80` is `-128`. If you add the offset to the address *of* the instruction instead of
the address *after* it, every loop is off by two — and the test names it for you.

### The condition codes

| Suffix | Takes the jump when | `JP` opcode | `JR` opcode |
| --- | --- | --- | --- |
| `NZ` | `Z == 0` | `0xC2` | `0x20` |
| `Z` | `Z == 1` | `0xCA` | `0x28` |
| `NC` | `C == 0` | `0xD2` | `0x30` |
| `C` | `C == 1` | `0xDA` | `0x38` |

Note `NC` means "no carry", i.e. `C == 0` — the flag is not set, the condition is true.
Getting `NC` backwards is a one-line bug that produces plausible-looking wrong loops.

### Why conditional instructions have two cycle counts

The operand bytes are fetched **either way** — the CPU does not know whether the jump
will be taken until it has decoded the instruction, and fetching is what costs time.
A not-taken `JP cc` is 12 T (the three-byte fetch) versus 16 T taken (the extra 4 for
writing `PC`). `JR cc` is 8 versus 12. This is why every opcode table in every
reference has a taken/not-taken column, and why your dispatcher returns different
numbers on the two paths.

### The program you are going to run

```
0100: 3E 00        LD A,0
0102: 06 0A        LD B,10
0104: 80           ADD A,B          <- loop
0105: 05           DEC B
0106: 20 FC        JR NZ,-4         (target 0x0104)
```

Trace it before you run it:

| iteration | A before | B before | after `ADD` | after `DEC` | `JR NZ` |
| --- | --- | --- | --- | --- | --- |
| 1 | `0x00` | `10` | A=`0x0A` | B=`9`, Z=0 | taken |
| 2 | `0x0A` | `9` | A=`0x13` | B=`8`, Z=0 | taken |
| ... | ... | ... | ... | ... | ... |
| 10 | `0x37-1` | `1` | A=`0x37` | B=`0`, Z=1 | **not taken** |

The sum is `10+9+...+1 = 55 = 0x37`. Instruction count: 2 setup + 10 x 3 = **32**, and
the final `PC` is `0x0108` because the last `JR` fell through. `m04_program_loop_sum`
runs exactly 32 `cpu_step`s and asserts `A`, `B`, `PC` and that the machine did not
fault. If your loop takes 31 or 33 steps, a flag or an offset is wrong — and the test
tells you which register disagrees.

> **What matters for the code you are about to write**
>
> * `JP cc,a16`: fetch the 16-bit address **first** (`cpu_fetch16`), then test the
>   condition, then assign `PC` only if taken. Return **16** taken, **12** not.
> * `JR cc,e8`: fetch the offset first, then `target = gb->cpu.pc + (int8_t)offset`
>   — `gb->cpu.pc` is already past the instruction, so no adjustment is needed.
>   Return **12** taken, **8** not. Unconditional `JR e8` is always 12.
> * Conditions: `NZ` = `!(f & GB_FLAG_Z)`, `Z` = `(f & GB_FLAG_Z)`, `NC` =
>   `!(f & GB_FLAG_C)`, `C` = `(f & GB_FLAG_C)`. All four compare against the flags
>   *as they are when the jump executes*.
> * Do not "optimise" by skipping the operand fetch on the not-taken path: the bytes
>   must be consumed or `PC` will be wrong, and the 12-vs-16 distinction exists to
>   catch exactly that.
> * `m04_program_loop_sum` counts **instructions**, not cycles, so a wrong cycle count
>   will not fail it — but it will fail `instr_timing` at L17. Get both right now.

---

## 2. Your task — 50 min

Work in `gb/src/opcodes.c`.

1. **Conditional `JP`** (15 min). Four opcodes, one helper that maps the opcode to a
   condition. `JP` is already implemented from L06 — reuse the same "assign `PC`"
   path and only add the test and the second cycle count.
2. **`JR`** (20 min). Unconditional `0x18` plus the four conditional forms. One helper
   for the condition, one line for the target:
   `gb->cpu.pc += (int8_t)cpu_fetch8(gb);` — that is a legitimate one-liner, but only
   because `cpu_fetch8` has already advanced `PC` past the offset. Write it in two
   steps the first time so you can see why.
3. **A regression check on `PC`** (5 min). Before running the tests, write a scratch case:
   `JR -2` at `0x0100` must land back on `0x0100`, and `JR 0` must land on `0x0102`.
   Both are in `m04_jp_jr_conditions`, but predicting them first is how you find the
   off-by-two before the harness does.
4. **Run the tests** (10 min):
   ```
   .\gb\build.cmd -Test m04
   ```
   `m04_jp_jr_conditions` and `m04_program_loop_sum` green. `m04_call_ret_stack`,
   `m04_push_pop_order`, `m04_rst_vectors` still red — that is the stack, L13.

---

## 3. Prove it — 20 min

```
.\gb\build.cmd -Test m04
```
```
[       OK ] m04_jp_jr_conditions
[  FAILED  ] m04_call_ret_stack      <- L13
[       OK ] m04_program_loop_sum
...
```
For the AHA, do not stop at green. Write a ten-line scratch harness that loads the loop
program, enables the tracer, runs 32 steps, and prints `gb_trace_print_last(gb, stdout,
12)` plus `A`, `B` and `PC`. You will see `ADD A,B`, `DEC B`, `JR NZ` cycling with
steadily growing `A` — **a real program executing on a CPU you wrote**, four lessons
before there is anything to plug a ROM into. Keep that scratch file; it is the fastest
way to debug any future opcode.

Then:
```
.\gb\build.cmd -Test m0
```
`m00`-`m04`: everything green except L13's three `m04_*` tests.

---

## 4. If it fails

| Symptom | Cause | Find it |
| --- | --- | --- |
| `JR -2` lands on `0x0102` | you added the offset to the instruction's address instead of the post-fetch `PC` | the one-line formula |
| Every not-taken `JP`/`JR` ends up at the wrong `PC` | you skipped the operand fetch on the not-taken path | fetch first, decide second |
| `JP NZ` and `JP Z` behave identically | you tested the wrong flag bit, or inverted the test | `NZ` is `!(f & 0x80)`, `Z` is `(f & 0x80)` |
| `NC` jumps when the carry is set | `NC` means `C == 0` | the flag is clear |
| The loop program ends with `A = 0x36` (54) | one iteration short — the `JR` at `B = 0` was taken | `DEC B` must set `Z` when `B` reaches 0 (L10) |
| The loop program ends at `PC = 0x0104` | it never fell through | same as above, or your `JR NZ` test is inverted |
| The loop never terminates and the test reports a timeout-ish failure | the offset is positive where it should be negative, or vice versa | `0xFC` is `-4`, not `+252` — cast to `int8_t` |
| `m04_program_loop_sum` reports a fault instead of a wrong value | a missing opcode hit `GB_UNIMPLEMENTED` | the message names the opcode and its address |

---

## 5. Done when

- [ ] `m04_jp_jr_conditions` and `m04_program_loop_sum` are green
- [ ] The scratch tracer run shows the loop cycling and `A` reaching `0x37`
- [ ] `-Test m0` shows the only remaining reds are the three stack tests
- [ ] You can explain why a not-taken conditional jump is not free
- [ ] You can compute the target of `JR 0xFE` at `0x0150` on paper (answer: `0x0150`)
- [ ] Commit message like `feat(cpu): conditional JP/JR and the first running program (L12)`

---

## 6. Optional, only if you have time

* `docs/02-cpu.md` §2.3 — the taken/not-taken convention across the whole instruction
  set, including `CALL cc` and `RET cc`, which you meet in L13.
* Extend the loop program with a counter that stops at 20 and verify the sum by hand
  (`20*21/2 = 210 = 0xD2`). Writing your own tiny programs is now the cheapest way to
  test new opcodes, and it needs no ROM and no PPU.

Next: **[L13 — The stack](L13-the-stack.md)**
