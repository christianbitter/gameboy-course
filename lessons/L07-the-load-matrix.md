# L07 — The load matrix

**Time** — theory ~20 min | coding ~60 min | verify ~10 min
**You will end with** — 64 opcodes implemented from a single rule, plus the memory
operand that makes them interesting. AHA: *the instruction set is not 512 facts, it
is about 40 facts and a lot of regularity.*
**Tests that must go green** — `m03_ld_r_r_matrix`
**Depends on** — L05 (`cpu_read_r8`/`cpu_write_r8`). You will edit `gb/src/opcodes.c`.

---

## 1. Read this — the whole theory for today

### One rule, 64 opcodes

Everything from `0x40` to `0x7F` is `LD dst,src`, where:

```
dst = (opcode >> 3) & 7
src =  opcode       & 7
```

and the operand space — call it `r8` — is indexed like this:

| index | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| operand | B | C | D | E | H | L | **(HL)** | A |

So `0x7F` is `dst = (0x7F>>3)&7 = 7` (A), `src = 0x7F&7 = 7` (A) -> `LD A,A`.
`0x46` -> `dst = 4` (H), `src = 6` ((HL)) -> `LD H,(HL)`.
`0x70` -> `dst = 6`, `src = 0` -> `LD (HL),B`.

That is the whole block. If you implement it as a loop over `dst` and `src` calling
`cpu_read_r8` and `cpu_write_r8`, you have written one instruction and got 63 for
free.

### The plus side of the same coin

`dst == 6` and `src == 6` would be `LD (HL),(HL)` — which has no meaning, and the
hardware uses that slot for **`HALT`** (`0x76`). Handle it explicitly, or your loop
will silently implement a no-op instruction that real hardware halts on.

### The memory operand costs extra time

Every `r8` access is a register except index 6, which is a memory access at `HL`:

| Shape | T-cycles |
| --- | --- |
| `LD r,r'` (neither is `(HL)`) | 4 |
| `LD r,(HL)` | 8 |
| `LD (HL),r` | 8 |

The extra 4 T-cycles (1 M-cycle) is the memory read or write. Note it is **one**
`(HL)` operand, never two — `0x76` is `HALT`, not `LD (HL),(HL)`. The cycle
arithmetic is asserted in L10 by `m03_hl_indirect_cycles`; implement it correctly
now so you do not have to revisit it.

### The one remaining load into memory

`0x36 LD (HL),d8` is not part of the `0x40-0x7F` block: it is the `(HL)` slot of the
`LD r,d8` family, and it costs **12 T-cycles** (fetch opcode, fetch operand, write
memory). `(opcode >> 3) & 7 == 6` in the `0x06/0x0E/.../0x3E` block.

> **What matters for the code you are about to write**
>
> * `dst = (op >> 3) & 7`, `src = op & 7`, operand order `B C D E H L (HL) A`.
> * The loop body is exactly: `cpu_write_r8(gb, dst, cpu_read_r8(gb, src));`.
>   Because `cpu_read_r8` runs first, `LD H,(HL)` reads memory *before* `H` changes
>   — which is the correct order and the reason the accessor pattern works at all.
> * `0x76` -> `GB_UNIMPLEMENTED("HALT - implemented in L15")`, **not** a store.
> * T-cycles: `4` when neither index is 6, `8` when either is, and `12` for `0x36`.
> * `m03_ld_r_r_matrix` checks the resulting **value** in every destination for all
>   49 legal pairs, including the `(HL)` ones against `0xC000`, so a wrong index
>   order will fail on a specific pair and name it for you.

---

## 2. Your task — 60 min

Work in `gb/src/opcodes.c`.

1. **The block as a loop** (25 min). In `op_execute`:
   ```c
   if (opcode >= 0x40 && opcode <= 0x7F) { ... }
   ```
   Compute `dst` and `src`, special-case `0x76`, call the accessors, and return the
   cycle count from the table above. Keep the mnemonic in a comment or a table if your
   design has one — L33's disassembler will thank you.
2. **`0x36 LD (HL),d8`** (10 min). `cpu_write_r8(gb, 6, cpu_fetch8(gb))`, 12 T.
   Note the operand is fetched *after* you know it is needed, which is why it cannot
   live inside the `0x40-0x7F` loop.
3. **Check the matrix by hand, quickly** (10 min). Before running the tests, satisfy
   yourself that `0x76`, `0x7E` (`LD A,(HL)`) and `0x46` (`LD H,(HL)`) all do what
   you think. This is the cheapest moment to catch an inverted `dst`/`src`.
4. **The remaining load forms** (10 min, optional today but required by L17):
   `0x02 LD (BC),A`, `0x12 LD (DE),A`, `0x0A LD A,(BC)`, `0x1A LD A,(DE)`,
   `0x22 LD (HL+),A`, `0x2A LD A,(HL+)`, `0x32 LD (HL-),A`, `0x3A LD A,(HL-)`.
   No unit test covers these today; `cpu_instrs` at L17 covers them all, so a mistake
   here is found then, not never. Each is 8 T.
5. **Run the tests** (5 min):
   ```
   .\gb\build.cmd -Test m03
   ```
   `m03_ld_r_r_matrix` green. `m03_hl_indirect_cycles` will still be red — it needs
   `ADD A,(HL)` (L08) and `INC (HL)` (L10). That is expected; the ladder owns that
   test by L10.

---

## 3. Prove it — 10 min

```
.\gb\build.cmd -Test m03
```
```
[       OK ] m03_ld_r_r_matrix
[  FAILED  ] m03_hl_indirect_cycles   <- needs L08 and L10, expected
```
Then read the `m03_ld_r_r_matrix` implementation once, and note that it iterates all
49 pairs and names the failing pair in the message. That is your instrument for this
lesson: if it says `LD L,(HL) (op 0x6E): got 0x00, want 0x77`, you know immediately
which half of the rule is wrong.

```
.\gb\build.cmd -Test m0
```
`m00`/`m01`/`m02` must stay green.

---

## 4. If it fails

| Symptom | Cause | Find it |
| --- | --- | --- |
| Every `(HL)` pair fails but every register pair passes | index 6 is not routed to the bus in your accessors | `cpu_read_r8(gb, 6)` must be `bus_read(gb, cpu_hl(&gb->cpu))` |
| The message says `LD L,(HL): got 0x00, want 0x77` | the fixture put `0x77` at `0xC000`; you read somewhere else | your `HL` handling, or you used `cpu_hl` before the fixture set `HL` |
| `LD H,(HL)` writes the *new* `H` twice | you wrote the destination before reading the source | one expression, read then write |
| The whole test hangs | you looped `dst`/`src` inside `op_execute` instead of using the opcode's own fields | decode from `opcode`, not from a counter |
| `0x76` behaves as `LD (HL),(HL)` | no special case | test it in a scratch program and watch the trace |
| A later test regresses with `UNIMPLEMENTED` | you replaced the default case while adding the block | the default `GB_UNIMPLEMENTED` must stay last |

---

## 5. Done when

- [ ] `m03_ld_r_r_matrix` is green in all 49 legal pairs
- [ ] `-Test m0` shows no regression
- [ ] You can derive `dst`/`src` for `0x46`, `0x70`, `0x7E` and `0x36` on paper
- [ ] You can explain why `0x76` cannot be part of the loop
- [ ] The eight `(BC)`/`(DE)`/`(HL+/-)` forms are either implemented or listed in `NOTES.md` as L17 debt
- [ ] Commit message like `feat(cpu): the LD r,r' matrix from one rule (L07)`

---

## 6. Optional, only if you have time

* `docs/02-cpu.md` §2.5 — the same rule stated as architecture, plus why the 8080
  heritage makes `(HL)` the "memory operand" of every block.
* Extend your `gb_disasm` to print the block, using the same rule. It is four lines
  and turns the tracer from hex into something you can read.

Next: **[L08 — Addition and carry](L08-addition-and-carry.md)**
