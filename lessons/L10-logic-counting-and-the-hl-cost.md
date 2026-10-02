# L10 — Logic, counting, and the `(HL)` cost

**Time** — theory ~25 min | coding ~55 min | verify ~10 min
**You will end with** — the rest of the 8-bit ALU, the counter instructions, and the
cycle accounting for read-modify-write memory operands. AHA: *`INC (HL)` costs 12 not
8, and I can say why.*
**Tests that must go green** — `m03_alu_logic_flags`, `m03_inc_dec_flags`,
`m03_hl_indirect_cycles`
**Depends on** — L07 (`LD` matrix and `(HL)` access), L08 (`ADD A,(HL)`). You will edit
`gb/src/opcodes.c`.

---

## 1. Read this — the whole theory for today

### The logic operations, and the one weird flag

`AND r`, `XOR r`, `OR r` are operations 4, 5 and 6 of the same ALU block, with
immediate forms `0xE6`, `0xEE`, `0xF6`:

```
res = A <op> n            /* bitwise */
Z   = (res == 0)
N   = 0
C   = 0
H   = (op == AND) ? 1 : 0        /* yes, really */
```

| instruction | Z | N | H | C |
| --- | --- | --- | --- | --- |
| `AND r` | `res == 0` | 0 | **1** | 0 |
| `XOR r` | `res == 0` | 0 | 0 | 0 |
| `OR r` | `res == 0` | 0 | 0 | 0 |

`AND` setting the half-carry flag is not a typo and not a bug in your emulator — it is
how the hardware's ALU is wired, and ROMs can and do observe it. `m03_alu_logic_flags`
contains a vector whose only purpose is to catch this.

All three **clear `C`** regardless of what it was. The fixture includes a vector with
`C` set for exactly that reason.

### `INC` and `DEC`: the only arithmetic that leaves `C` alone

`INC r` and `DEC r` are `0x04/0x0C/0x14/0x1C/0x24/0x2C/0x34/0x3C` and
`0x05/0x0D/0x15/0x1D/0x25/0x2D/0x35/0x3D` — the `(op & 0xC7) == 0x04` and
`== 0x05` families, with index `(op >> 3) & 7`:

```
INC r:  res = (r + 1) & 0xFF;   Z = (res == 0);  N = 0;  H = ((r & 0xF) == 0xF);  C unchanged
DEC r:  res = (r - 1) & 0xFF;   Z = (res == 0);  N = 1;  H = ((r & 0xF) == 0x0);  C unchanged
```

Read the `H` rules carefully: they are *not* the general carry/borrow tests. `INC`
sets `H` when the low nibble was `0xF` (it is about to carry out of bit 3), and `DEC`
sets `H` when the low nibble was `0x0` (it is about to borrow into bit 4).

**`C` is preserved.** That is the single most-tested property here: the fixtures set
`C` in the incoming flags for the `INC`/`DEC` vectors and expect it to survive. If you
build the flag byte from zero like you did for `ADD`, you will destroy it — this block
is the exception to "clear `F` first". Save `C`, build the new flags, restore the bit.

### The `(HL)` cost, and why `INC (HL)` is 12

Every `(HL)` operand is a bus access, and a bus access is 1 M-cycle = 4 T-cycles:

| Shape | T-cycles | Why |
| --- | --- | --- |
| `INC r` / `DEC r` | 4 | fetch only |
| `AND r` / `OR r` / `XOR r` | 4 | fetch only |
| `AND (HL)` / `ADD A,(HL)` / `LD r,(HL)` | 8 | fetch + read |
| `LD (HL),r` | 8 | fetch + write |
| `INC (HL)` / `DEC (HL)` | **12** | fetch + **read** + **write** |

`INC (HL)` is a read-modify-write: the CPU must read the byte, increment it, and write
it back, hence two memory accesses. `m03_hl_indirect_cycles` asserts all four shapes
(4, 8, 12) and is owned by this lesson because it needs `INC (HL)`.

### Worked examples — the test vectors

| Instruction | input | flags in | result | flags out |
| --- | --- | --- | --- | --- |
| `AND B` | A=`0xF0`, B=`0x0F` | `0x00` | `0x00` | `0xA0` (Z, H) |
| `AND B` | A=`0xFF`, B=`0x0F` | `0x00` | `0x0F` | `0x20` (H only) |
| `AND B` | A=`0xF0`, B=`0x70` | `0x10` (C set) | `0x70` | `0x20` — `C` cleared, `H` set |
| `XOR B` | A=`0xFF`, B=`0xFF` | `0x00` | `0x00` | `0x80` |
| `XOR B` | A=`0x0F`, B=`0xF0` | `0x10` | `0xFF` | `0x00` |
| `OR B` | A=`0x0F`, B=`0xF0` | `0x00` | `0xFF` | `0x00` |
| `OR B` | A=`0x00`, B=`0x00` | `0x10` | `0x00` | `0x80` — `C` cleared |
| `INC B` | B=`0x0F` | `0x10` (C set) | `0x10` | `0x30` — H and **C preserved** |
| `INC B` | B=`0xFF` | `0x00` | `0x00` | `0xA0` — Z, H |
| `INC B` | B=`0x00` | `0x10` | `0x01` | `0x10` — only the preserved C |
| `DEC B` | B=`0x01` | `0x10` | `0x00` | `0xD0` — Z, N, **C preserved** |
| `DEC B` | B=`0x10` | `0x00` | `0x0F` | `0x60` — N, H |
| `DEC B` | B=`0x00` | `0x00` | `0xFF` | `0x60` — N, H |

> **What matters for the code you are about to write**
>
> * `AND` sets `H = 1`. `XOR`/`OR` clear it. All three clear `C`. `N = 0` for all three.
> * `INC`/`DEC` **preserve `C`**: read it before you rebuild `F`, then OR it back in.
>   Building `F` from zero — correct for `ADD`/`SUB` — is wrong here.
> * `INC`'s `H` is `(old & 0xF) == 0xF`; `DEC`'s `H` is `(old & 0xF) == 0x0`. Use the
>   **old** value, before the increment.
> * `INC (HL)`/`DEC (HL)` are read-modify-write: `cpu_read_r8(gb, 6)`, modify, then
>   `cpu_write_r8(gb, 6, res)`. Two accesses, **12 T**.
> * `m03_hl_indirect_cycles` also re-checks `LD (HL),d8` (12), `LD A,(HL)` (8),
>   `LD (HL),B` (8) and `ADD A,(HL)` (8) — if one of those is red, the regression is in
>   an earlier lesson, not in today's work.

---

## 2. Your task — 55 min

Work in `gb/src/opcodes.c`.

1. **`AND`/`XOR`/`OR`** (20 min). Operations 4/5/6 of the ALU block plus `0xE6`, `0xEE`,
   `0xF6`. Remember the `AND`-sets-`H` rule and that all three clear `C`.
2. **`INC r`/`DEC r`** (20 min). Twelve opcodes each of the `(op & 0xC7) == 0x04/0x05`
   family, parameterised by `(op >> 3) & 7` — so the same loop you wrote for `LD r,r'`
   and the ALU works here. Preserve `C`.
3. **`INC (HL)`/`DEC (HL)`** (10 min). Index 6 is the `(HL)` case; make it a
   read-modify-write and return 12. Index 6 of `INC r` is `0x34`, of `DEC r` is `0x35`.
4. **Run the tests** (5 min):
   ```
   .\gb\build.cmd -Test m03
   ```
   `m03_alu_logic_flags`, `m03_inc_dec_flags` and `m03_hl_indirect_cycles` green.
   Only `m03_rotates_a` and `m03_daa` remain red — L11.
5. **Confirm the cycle table** (5 min). After the suite is green, print the T-cycles for
   `LD A,(HL)`, `LD (HL),B`, `INC (HL)` and `ADD A,(HL)` from a scratch test. Seeing
   8/8/12/8 in one place is the moment the `(HL)` cost stops being folklore.

---

## 3. Prove it — 10 min

```
.\gb\build.cmd -Test m03
```
```
[       OK ] m03_alu_logic_flags
[       OK ] m03_inc_dec_flags
[       OK ] m03_hl_indirect_cycles
[  FAILED ] m03_daa              <- L11
[  FAILED ] m03_rotates_a        <- L11
```
`m03_hl_indirect_cycles` is the interesting one: it is the only test in the suite so far
that asserts **timing** as well as values, and it passes only if every `(HL)` path you
have built agrees on the same rule.

```
.\gb\build.cmd -Test m0
```
Regression sweep: `m00`-`m03` should be green apart from L11's two tests.

---

## 4. If it fails

| Symptom | Cause | Find it |
| --- | --- | --- |
| `m03_inc_dec_flags` fails only where `C` was set | you rebuilt `F` from zero, losing `C` | read and restore the carry bit |
| `INC B 0xFF -> 00` gets `H` clear | you tested the *new* nibble instead of the old one | `H` uses the value before the increment |
| `DEC B 0x10 -> 0F` gets `H` clear | `DEC`'s `H` is `(old & 0xF) == 0`, i.e. it fires on `0x10`, not on `0x0F` | check the rule's direction |
| `AND` fails the `H`-only vector | you cleared `H` like the other logic ops | `AND` sets `H = 1` |
| `XOR`/`OR` leave `C` set | you preserved `C` for all of the block | only `INC`/`DEC` preserve it |
| `m03_hl_indirect_cycles` is 8 where 12 is expected | `INC (HL)` wrote without reading, or you counted one access | read, modify, write |
| The `(HL)` logic ops cost 8 and fail timing | the `+4` is only in the load path | the penalty belongs to any bus access via index 6 |

---

## 5. Done when

- [ ] All three tests for this lesson are green, so only L11's two remain red in `m03`
- [ ] `-Test m0` shows no regression
- [ ] You can state which single 8-bit ALU instruction sets `H` for no arithmetic reason
- [ ] You can explain why `INC (HL)` is 12 T and `INC B` is 4 T
- [ ] You can explain why `INC`/`DEC` must not clear `C`
- [ ] Commit message like `feat(alu): logic ops, INC/DEC, (HL) read-modify-write (L10)`

---

## 6. Optional, only if you have time

* `docs/02-cpu.md` §2.4 — the whole flag table, so you can see how unusual today's two
  rules are next to the rest of the family.
* Add the 16-bit `INC rr`/`DEC rr` forms now (`0x03/0x13/0x23/0x33`, `0x0B/0x1B/0x2B/0x3B`):
  8 T, and they touch **no flags at all**, not even `Z`. That is L13's territory but it
  is two minutes from here.

Next: **[L11 — Rotates and BCD](L11-rotates-and-bcd.md)**
