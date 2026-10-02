# L08 — Addition and carry

**Time** — theory ~25 min | coding ~55 min | verify ~10 min
**You will end with** — the first arithmetic, and the flag rules that decide whether
your emulator is correct or merely plausible. AHA: *flags are derived values, and I
can state the rule for each one.*
**Tests that must go green** — `m03_alu_add_flags`, `m03_alu_adc_flags`
**Depends on** — L07 (the `r8` accessors and the matrix). You will edit
`gb/src/opcodes.c`.

---

## 1. Read this — the whole theory for today

### The ALU block has the same shape as the load block

`0x80-0xBF` is "an ALU operation between `A` and an `r8` operand":

```
operation = (opcode - 0x80) >> 3      /* 0..7 */
operand   =  opcode        & 7        /* the same B C D E H L (HL) A space */
```

| operation | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| instruction | `ADD A,r` | `ADC A,r` | `SUB r` | `SBC A,r` | `AND r` | `XOR r` | `OR r` | `CP r` |

Today you implement `ADD` and `ADC` (`0x80-0x8F`). L09 takes `SUB`/`SBC`/`CP`.
The immediate forms take the operand from the byte after the opcode:

| Opcode | Instruction | Bytes | T-cycles |
| --- | --- | --- | --- |
| `0x80-0x87` | `ADD A,r` | 1 | 4, or 8 when `r` is `(HL)` |
| `0x88-0x8F` | `ADC A,r` | 1 | 4, or 8 when `r` is `(HL)` |
| `0xC6` | `ADD A,d8` | 2 | 8 |
| `0xCE` | `ADC A,d8` | 2 | 8 |

There is a regularity worth memorising, because your own code and the tests both
depend on it: **the immediate ALU forms are exactly the opcodes where
`(op & 0xC7) == 0xC6`** — `C6 CE D6 DE E6 EE F6 FE`. That is also how the test
fixture knows whether to build a one- or two-byte program.

### The flags, precisely

`ADD`:

```
res = (A + n) & 0xFF
Z   = (res == 0)
N   = 0
H   = ((A & 0xF) + (n & 0xF)) > 0xF          /* carry out of bit 3  */
C   = (A + n) > 0xFF                         /* carry out of bit 7  */
```

`ADC` is the same with `carry` added to **three** places — the result, the half-carry
test, and the carry test:

```
res = (A + n + carry) & 0xFF
Z   = (res == 0)
N   = 0
H   = ((A & 0xF) + (n & 0xF) + carry) > 0xF
C   = (A + n + carry) > 0xFF
```

The incoming `C` flag is `(gb->cpu.f & GB_FLAG_C) != 0`. `Z` is recomputed from the
result, never carried over. `N` is explicitly cleared by both.

### Worked examples — these are literally the test vectors

| Instruction | A | operand | carry in | result | F | why |
| --- | --- | --- | --- | --- | --- | --- |
| `ADD A,B` | `0x0F` | `0x01` | — | `0x10` | `0x20` | nibble sum `0x10 > 0xF` -> H |
| `ADD A,B` | `0x00` | `0x00` | — | `0x00` | `0x80` | Z only |
| `ADD A,B` | `0xFF` | `0x01` | — | `0x00` | `0xB0` | Z, H, C |
| `ADD A,B` | `0x3A` | `0xC6` | — | `0x00` | `0xB0` | `0xA + 0x6 = 0x10` -> H, sum `0x100` -> C |
| `ADD A,B` | `0x12` | `0x34` | — | `0x46` | `0x00` | nothing |
| `ADC A,B` | `0x0F` | `0x00` | 1 | `0x10` | `0x20` | H; the incoming carry is *consumed*, so C is cleared |
| `ADC A,B` | `0xFF` | `0x00` | 1 | `0x00` | `0xB0` | Z, H, C |
| `ADC A,B` | `0x10` | `0x20` | 1 | `0x31` | `0x00` | nothing |
| `ADC A,d8` | `0x0F` | `0x01` | 1 | `0x11` | `0x20` | H; again C cleared |

### Compute in `int`, store in `u8`

Do the arithmetic in `int` (or `unsigned`) and truncate only on store. If you compute
in `u8`, the carry out of bit 7 is destroyed before you can test it, and you will
spend an hour wondering why only the carry cases fail.

> **What matters for the code you are about to write**
>
> * Write one helper — `add_with_carry(gb, n)` and `adc_with_carry(gb, n)`, or a single
>   helper taking the carry bit — and use it for both the register and the immediate
>   forms. Four call sites, one rule.
> * `H` uses the **nibbles**, `C` uses the **full byte**. Both must include the
>   incoming carry for `ADC`.
> * `Z` is `res == 0` and `N` is always cleared. Clear the whole flag byte before
>   setting bits, so you can never inherit a stale flag.
> * The operand comes from `cpu_read_r8(gb, op & 7)` for the register forms and
>   `cpu_fetch8(gb)` for `0xC6`/`0xCE` — and for `(HL)` the accessor costs 4 extra
>   T-cycles, so `ADD A,(HL)` is 8.
> * The fixture's second operand is `B` for register forms (`{op, a, b, f, ...}`) and
>   the **immediate byte** for the `d8` forms, which is why the same field is used for
>   both. Implement `(op & 0xC7) == 0xC6` as two-byte instructions and the vectors
>   line up.

---

## 2. Your task — 55 min

Work in `gb/src/opcodes.c`.

1. **Extend the ALU block skeleton** (10 min). You now have two of the eight
   operations. Write the shared decode (`op - 0x80`, `op & 7`) and switch on the
   operation; leave `SUB`..`CP` returning `GB_UNIMPLEMENTED("ALU op %02X (L09)")`.
2. **The addition helper** (20 min). One function, computing `res`, `Z`, `N`, `H`, `C`
   per the formulas above, writing `gb->cpu.a` and `gb->cpu.f`, returning the T-cycles
   for the shape (4 register, 8 `(HL)`).
3. **Wire the four forms** (15 min): `0x80-0x87` (`ADD A,r`), `0x88-0x8F`
   (`ADC A,r`), `0xC6` (`ADD A,d8`), `0xCE` (`ADC A,d8`). For `ADC`, read the
   incoming carry **before** you overwrite `F`.
4. **Run the tests** (10 min):
   ```
   .\gb\build.cmd -Test m03_alu
   ```
   `m03_alu_add_flags` and `m03_alu_adc_flags` green. `m03_alu_sub_flags`,
   `m03_alu_sbc_flags`, `m03_alu_cp_no_write`, `m03_alu_logic_flags`,
   `m03_inc_dec_flags`, `m03_rotates_a`, `m03_daa` still red — L09-L11.
5. **Probe the boundary on purpose** (5 min). In a scratch copy, extend the case table
   with `ADD A,B 0x7F + 0x01` (expect `0x80`, `F = 0x20`: H only, no C) and
   `ADD A,B 0x80 + 0x80` (expect `0x00`, `F = 0x90`: Z and C, no H). If both pass, your
   bit-3 and bit-7 boundaries are genuinely separate in your code.

---

## 3. Prove it — 10 min

```
.\gb\build.cmd -Test m03_alu
```
```
[       OK ] m03_alu_add_flags
[       OK ] m03_alu_adc_flags
[  FAILED  ] m03_alu_sub_flags      <- L09
...
```
Each failing case message prints the case name, for example
`ADC A,B FF+00+C -> ZHC: A=0x00 (want 0x00)  F=0xB0 (want 0xB0)`. When one fails, the
name tells you which rule you got wrong, so read the name before the numbers.

```
.\gb\build.cmd -Test m0
```
Regression check: `m00`-`m02` and the rest of `m03` must be no worse than before.

---

## 4. If it fails

| Symptom | Cause | Find it |
| --- | --- | --- |
| Only the carry-out cases fail | you computed in `u8`, so `A + n` wrapped before you tested `> 0xFF` | compute in `int`, truncate on store |
| Only `ADC` fails, and only where a carry enters | you added the carry to the result but not to the `H`/`C` tests (or vice versa) | all three places |
| `Z` is wrong when the result is `0xFF` | you set `Z` from the carry, or from `A` before the operation | `Z = (res == 0)` |
| `N` is set after `ADD` | you forgot to clear the flag byte | build `f` from zero each time |
| `H` is right but `C` is wrong on `0x3A + 0xC6` | you tested bit 7 of the *nibble* | `C` is `(A + n) > 0xFF`, full width |
| `ADC A,d8` reads `0x00` as its operand | you treated `0xCE` as a one-byte instruction | the immediate forms are `(op & 0xC7) == 0xC6` |
| `ADD A,(HL)` costs 4 instead of 8 | the `(HL)` penalty is missing from the ALU path | the `+4` belongs wherever `op & 7 == 6` is read through the bus |

---

## 5. Done when

- [ ] `m03_alu_add_flags` and `m03_alu_adc_flags` are green
- [ ] Your own two extra boundary vectors (`0x7F+1`, `0x80+0x80`) pass without changing code
- [ ] `-Test m0` shows no regression
- [ ] You can state all four flag rules for `ADD` and `ADC` out loud
- [ ] You can explain why `ADC` computes `H` from nibbles *plus* carry
- [ ] Commit message like `feat(alu): ADD and ADC with full flag rules (L08)`

---

## 6. Optional, only if you have time

* `docs/02-cpu.md` §2.4 — the complete flag table for all eight ALU families, showing
  that `AND` sets `H` (the surprising one) and that `INC`/`DEC` leave `C` alone.
* Add the two `(HL)` ALU forms to a scratch test now; `m03_hl_indirect_cycles` will
  assert `ADD A,(HL)` costs 8 at L10.

Next: **[L09 — Subtraction and compare](L09-subtraction-and-compare.md)**
