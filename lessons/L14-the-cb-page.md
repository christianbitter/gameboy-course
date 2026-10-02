# L14 — The CB page

**Time** — theory ~25 min | coding ~50 min | verify ~15 min
**You will end with** — the second half of the instruction set: eight shifts and
rotates, and `BIT`/`RES`/`SET`, all 256 opcodes, from four rules.
**Tests that must go green** — `m04_cb_rotates`, `m04_cb_bit_res_set`
**Depends on** — L07 (the `r8` accessors). You will edit `gb/src/opcodes.c`.

---

## 1. Read this — the whole theory for today

### `0xCB` is a prefix, not an instruction

`0xCB` means "the next byte is the real opcode". The instruction is always **two
bytes**: `0xCB` then the sub-opcode. So:

* your dispatcher receives `0xCB`, consumes it, then fetches the sub-opcode itself
  (`u8 cb = cpu_fetch8(gb);`),
* `last_opcode_len` must be **2** for every CB instruction (the tracer and any future
  disassembler depend on it),
* the CB operand space is the same `r8` you already have: `B C D E H L (HL) A`.

### Four rules, 256 opcodes

| Sub-opcode range | Meaning | Formula |
| --- | --- | --- |
| `0x00-0x3F` | rotate/shift | operation `= (cb >> 3) & 7`, target `= cb & 7` |
| `0x40-0x7F` | `BIT n,r` | `n = (cb >> 3) & 7`, target `= cb & 7` |
| `0x80-0xBF` | `RES n,r` | `n = (cb >> 3) & 7`, target `= cb & 7` |
| `0xC0-0xFF` | `SET n,r` | `n = (cb >> 3) & 7`, target `= cb & 7` |

The operation order for the shift block is fixed and worth memorising:

| `(cb>>3)&7` | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| instruction | `RLC` | `RRC` | `RL` | `RR` | `SLA` | `SRA` | `SWAP` | `SRL` |

So `0xCB 0x37` is `SWAP A` (`(0x37>>3)&7 = 6` -> SWAP, `0x37&7 = 7` -> A) and
`0xCB 0x86` is `RES 0,(HL)`.

### The eight operations

| Instruction | Effect | Carry out |
| --- | --- | --- |
| `RLC r` | `r = (r << 1) \| (r >> 7)` | old bit 7 |
| `RRC r` | `r = (r >> 1) \| (r << 7)` | old bit 0 |
| `RL r` | `r = (r << 1) \| carry` | old bit 7 |
| `RR r` | `r = (r >> 1) \| (carry << 7)` | old bit 0 |
| `SLA r` | `r = r << 1` (0 shifted in) | old bit 7 |
| `SRA r` | `r = (r >> 1) \| (r & 0x80)` (sign kept) | old bit 0 |
| `SWAP r` | `r = (r >> 4) \| (r << 4)` (nibbles swapped) | — (`C = 0`) |
| `SRL r` | `r = r >> 1` (0 shifted in) | old bit 0 |

Flags for all eight:

```
Z = (result == 0)      /* yes, here it IS computed from the result      */
N = 0
H = 0
C = the bit shifted out (0 for SWAP)
```

**This is the opposite of L11's `RLCA` family.** `RLCA` always clears `Z` and exists
only for `A`; `RLC r` sets `Z` from the result and works on any `r8`. Every reference
table lists both, and mixing their flag rules is a classic error.

### `BIT`, `RES`, `SET`

```
BIT n,r:   Z = ((r & (1 << n)) == 0);  N = 0;  H = 1;  C unchanged
RES n,r:   r &= ~(1 << n);             /* no flags at all */
SET n,r:   r |=  (1 << n);             /* no flags at all */
```

`BIT` sets `H = 1` — like `AND` in L10, it is a hardware artefact, not arithmetic.
And `BIT` leaves `C` alone: `m04_cb_bit_res_set` supplies `0x10` in the incoming flags
and expects it in the output for every `BIT` vector. `RES`/`SET` touch nothing.

### Cycle counts: the prefix is included

This course's convention (written down in `reference/cheatsheet-flags-and-timing.md`)
is that the totals **include** the 4 T-cycles of the prefix fetch:

| Shape | Total T-cycles |
| --- | --- |
| `RLC r` … `SRL r`, `RES`/`SET` on a register | 8 |
| anything above on `(HL)` | 16 |
| `BIT r` on a register | 8 |
| `BIT (HL)` | 12 |

Note the asymmetry: `BIT (HL)` is **12**, not 16, because `BIT` reads the byte but does
not write it back. `RES (HL)`/`SET (HL)` are read-modify-write, so they are 16. Your
`op_execute(0xCB)` returns these totals; it does **not** add 4 for the prefix again.

### Worked examples — the test vectors

| Instruction | in | flags in | out | flags out |
| --- | --- | --- | --- | --- |
| `RLC B` | B=`0x80` | `0x00` | `0x01` | `0x10` |
| `RLC B` | B=`0x00` | `0x00` | `0x00` | `0x80` — **`Z` set, unlike `RLCA`** |
| `RRC B` | B=`0x01` | `0x00` | `0x80` | `0x10` |
| `RL B` | B=`0x80` | `0x00` (C=0) | `0x00` | `0x90` — Z and C |
| `RR B` | B=`0x01` | `0x10` (C=1) | `0x80` | `0x10` |
| `SLA B` | B=`0x80` | `0x00` | `0x00` | `0x90` |
| `SRA B` | B=`0x81` | `0x00` | `0xC0` | `0x10` — bit 7 preserved |
| `SWAP B` | B=`0xAB` | `0x00` | `0xBA` | `0x00` |
| `SRL B` | B=`0x01` | `0x00` | `0x00` | `0x90` |
| `BIT 7,A` | A=`0x80` | `0x10` | `0x80` | `0x30` — Z clear, H set, C kept |
| `BIT 7,A` | A=`0x00` | `0x10` | `0x00` | `0xB0` — Z set, H set, C kept |
| `SET 7,A` | A=`0x00` | `0x00` | `0x80` | `0x00` |
| `RES 0,A` | A=`0xFF` | `0x00` | `0x7F` | `0x00` |
| `RES 0,(HL)` | `[HL]=0xFF` | — | `[HL]=0xFE` | — (16 T) |
| `BIT 7,(HL)` | `[HL]=0xFF` | `0x00` | `[HL]=0xFF` | `0x20` (12 T) |

> **What matters for the code you are about to write**
>
> * `cpu_fetch8` the sub-opcode inside your `0xCB` handler, and set
>   `gb->cpu.last_opcode_len = 2`.
> * Decode with `n = (cb >> 3) & 7` and `target = cb & 7`; for the shift block the
>   operation is also `(cb >> 3) & 7`, in the order `RLC RRC RL RR SLA SRA SWAP SRL`.
> * The eight shifts set `Z` from the result. **`RLCA`/`RLA` do not** — different
>   instruction, different flags (L11).
> * `BIT` sets `H = 1` and leaves `C` untouched. `RES`/`SET` change no flags at all.
> * Cycle totals already include the prefix: register 8, `(HL)` 16, `BIT (HL)` 12.
>   `BIT (HL)` reads without writing, which is why it is not 16.
> * `SWAP`'s carry is a constant 0 — do not compute it from the result.
> * Reuse `cpu_read_r8`/`cpu_write_r8` for `target`, so `(HL)` costs the extra time in
>   exactly one place.

---

## 2. Your task — 50 min

Work in `gb/src/opcodes.c`.

1. **The prefix handler** (10 min). Add `case 0xCB` (or the table entry): fetch the
   sub-opcode, set `last_opcode_len = 2`, dispatch on the four ranges, return the totals
   from the table. Keep it as a separate function — it will be 40 lines and your main
   dispatcher should stay readable.
2. **The eight shifts** (20 min). One helper per operation is clearer than one clever
   helper. Watch `SRA` (bit 7 preserved) and `SWAP` (`C = 0`).
3. **`BIT`/`RES`/`SET`** (15 min). `BIT` writes only flags; `RES`/`SET` write only the
   target. That asymmetry is also the cycle-count asymmetry.
4. **Run the tests** (5 min):
   ```
   .\gb\build.cmd -Test m04
   ```
   All **seven** `m04_*` tests green. You have now implemented every control-flow and
   bit-manipulation instruction the hardware has.

---

## 3. Prove it — 15 min

```
.\gb\build.cmd -Test m04
```
```
   7 passed,    0 failed,    0 skipped
ALL GREEN
```
Then prove the page is *complete*, not just green. Write a scratch test that walks all
256 sub-opcodes: for each `cb`, execute it once with a harmless register state and
assert the machine did not fault. Any sub-opcode you forgot will print its own address
via `GB_UNIMPLEMENTED`, so this is a 20-line test that guarantees coverage rather than
trusting the four rules you read.

```
.\gb\build.cmd -Test m0
```
`m00`-`m04` should now be **fully green**. The next red test is in `m05` (interrupts and
the timer, L15-L16).

---

## 4. If it fails

| Symptom | Cause | Find it |
| --- | --- | --- |
| `m04_cb_rotates` fails only `RLC B` with `0x00` | you hard-zeroed `Z` the way L11's `RLCA` does | CB shifts set `Z` from the result |
| `SRA B` gives `0x40` for `0x81` | you shifted in 0 instead of preserving bit 7 | `SRA` is the arithmetic shift |
| `SWAP B` sets `C` | you computed the carry from the nibble swap | `C = 0` for `SWAP` |
| `BIT` clears `H` | you built `F` from zero | `BIT` sets `H = 1` and keeps `C` |
| `BIT 7,A` with `C` set returns `F = 0x20` | you cleared `C` | read `F`, set `Z` and `H`, keep the rest |
| `SET`/`RES` change `Z` | you routed them through a flag-writing helper | they change no flags |
| `m04_cb_bit_res_set` times `RES 0,(HL)` at 8 or 24 | you added the prefix twice, or used the `BIT` total for a write | inclusive totals 8 / 16 / 12 |
| `last_opcode_len` is 1 for CB instructions | you never set it in the prefix handler | 2, always |

---

## 5. Done when

- [ ] All seven `m04_*` tests are green, and `-Test m0` shows `m00`-`m04` fully green
- [ ] Your scratch walk over all 256 sub-opcodes finds no `UNIMPLEMENTED`
- [ ] You can derive the operation and target for `0xCB 0x37`, `0x86` and `0x4F` on paper
- [ ] You can say which shift preserves bit 7, and which two instructions in the whole
      ISA set `H` "for no reason"
- [ ] You can explain why `BIT (HL)` is 12 T while `RES (HL)` is 16 T
- [ ] Commit message like `feat(cpu): the whole CB page from four rules (L14)`

---

## 6. Optional, only if you have time

* `docs/02-cpu.md` §2.3 — the CB-prefix cycle-count convention, and why other references
  may quote 4/12/8 instead of 8/16/12 for the same instructions.
* Extend `gb_disasm` to decode the CB page using today's four rules. It is the last
  piece needed for the tracer to print real mnemonics for every opcode you have.

Next: **[L15 — Interrupts and HALT](L15-interrupts-and-halt.md)**
