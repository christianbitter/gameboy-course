# L11 — Rotates and BCD

**Time** — theory ~25 min | coding ~55 min | verify ~10 min
**You will end with** — the four fast rotates, the three flag-manipulation
instructions, and `DAA`, which is the only instruction whose behaviour depends on
*previous* flags to correct a decimal result. AHA: *I can implement a spec sentence
literally and see it work.*
**Tests that must go green** — `m03_rotates_a`, `m03_daa`
**Depends on** — L09 (borrow flags, because `DAA` reads `N`). You will edit
`gb/src/opcodes.c`.

---

## 1. Read this — the whole theory for today

### The four fast rotates

These are single-byte, 4 T-cycle instructions that rotate **A only**:

| Opcode | Instruction | Meaning | Carry out |
| --- | --- | --- | --- |
| `0x07` | `RLCA` | `A = (A << 1) \| (A >> 7)` | old bit 7 |
| `0x0F` | `RRCA` | `A = (A >> 1) \| (A << 7)` | old bit 0 |
| `0x17` | `RLA` | `A = (A << 1) \| carry` | old bit 7 |
| `0x1F` | `RRA` | `A = (A >> 1) \| (carry << 7)` | old bit 0 |

The flags are the surprise:

```
Z = 0     /* ALWAYS. Not "if the result is zero" — always zero.   */
N = 0
H = 0
C = the bit that rotated out
```

`m03_rotates_a` contains `RLCA 0x00 -> 0x00` with `F = 0x00`. A result of zero with `Z`
clear is impossible in every other arithmetic instruction you have written, and it is
the single most common bug in this lesson. `RLA`/`RRA` use the incoming carry as the
bit shifted in — and still clear `Z`, which is why the `RLA 0x00` with `C = 1` vector
expects `F = 0x00` even though the result is `0x01`.

The `0xCB`-prefixed rotates (`RLC B` and friends, L14) look identical but **do** set
`Z`, and they work on any `r8`. Keep the two families mentally separate.

### The three flag instructions

| Opcode | Instruction | Effect on A | Z | N | H | C |
| --- | --- | --- | --- | --- | --- | --- |
| `0x2F` | `CPL` | `A = ~A` | unchanged | 1 | 1 | unchanged |
| `0x37` | `SCF` | — | unchanged | 0 | 0 | 1 |
| `0x3F` | `CCF` | — | unchanged | 0 | 0 | `!C` |

"Unchanged" means exactly that: for `CPL` with `F = 0x90` (Z and C set) the result is
`F = 0xF0` — `N` and `H` set on top, `Z` and `C` left alone. Same shape for `SCF` and
`CCF`.

### `DAA` — decimal adjust, and why `N` exists

Real games keep scores in **BCD**: each nibble is one decimal digit, so `0x42` means
"42". Adding BCD values with the normal binary `ADD` gives the wrong digits past 9:
`0x15 + 0x27` should be `0x42` (15 + 27 = 42) but binary addition gives `0x3C`. `DAA`
repairs the accumulator after the fact, and it needs to know whether the previous
operation was an addition or a subtraction — that is what the `N` flag you have been
setting all along is *for*.

The algorithm, exactly:

```
if N == 0:                                   /* after ADD/ADC */
    if C == 1 or A > 0x99:  A += 0x60;  C = 1
    if H == 1 or (A & 0x0F) > 0x09:  A += 0x06
else:                                        /* after SUB/SBC */
    if C == 1:  A -= 0x60
    if H == 1:  A -= 0x06

A &= 0xFF
Z = (A == 0)
H = 0
/* N unchanged, C as set above */
```

Two details that decide whether your implementation passes:

1. **The high-nibble correction is `0x60`, the low-nibble correction is `0x06`.**
   Writing `0x06` twice is the classic bug: it passes the `0x3C -> 0x42` case and fails
   `0x9A -> 0x00`.
2. **The low-nibble test uses `A` *after* the high correction**, exactly as written
   above. That is what makes `0x9A` work: `0x9A > 0x99` so `A += 0x60 -> 0xFA`, and
   then `(0xFA & 0x0F) = 0x0A > 9` so `A += 0x06 -> 0x00` with `C` set. Do it in the
   other order and you get `0xA0`, which is wrong.

### Worked examples — the test vectors

| Instruction | A in | F in | A out | F out | why |
| --- | --- | --- | --- | --- | --- |
| `RLCA` | `0x85` | `0x00` | `0x0B` | `0x10` | high bit rotated into carry |
| `RLCA` | `0x00` | `0x00` | `0x00` | `0x00` | **`Z` stays clear** |
| `RRCA` | `0x85` | `0x00` | `0xC2` | `0x10` | low bit into carry |
| `RLA` | `0x85` | `0x00` | `0x0A` | `0x10` | carry 0 shifted in |
| `RLA` | `0x00` | `0x10` | `0x01` | `0x00` | carry 1 shifted in, `Z` still clear |
| `RRA` | `0x85` | `0x10` | `0xC2` | `0x10` | carry 1 into bit 7 |
| `CPL` | `0x35` | `0x90` | `0xCA` | `0xF0` | `N`,`H` set; `Z`,`C` kept |
| `SCF` | `0x00` | `0x80` | `0x00` | `0x90` | `C` set, `Z` kept |
| `CCF` | `0x00` | `0x90` | `0x00` | `0x80` | `C` 1 -> 0 |
| `DAA` | `0x9A` | `0x00` | `0x00` | `0x90` | high then low correction, `C` set |
| `DAA` | `0x3C` | `0x00` | `0x42` | `0x00` | low nibble correction only |
| `DAA` | `0x2D` | `0x60` | `0x27` | `0x40` | after `SUB`: `H` -> subtract `0x06` |
| `DAA` | `0xFF` | `0x70` | `0x99` | `0x50` | after `SUB` with carry |
| `DAA` | `0x00` | `0x20` | `0x06` | `0x00` | `H` set, no low-nibble overflow |

> **What matters for the code you are about to write**
>
> * `RLCA`/`RRCA`/`RLA`/`RRA` set `Z = 0` **unconditionally**. Do not compute `Z` from
>   the result, and do not "simplify" by reusing your `ADD` flag helper.
> * `RLA`/`RRA` read the incoming carry first, then overwrite `F`.
> * `CPL`, `SCF`, `CCF` change only the bits the table says: read `F`, set or clear
>   those bits, write it back. `Z` and (for `CPL`) `C` survive.
> * `DAA`: `0x60` for the high nibble, `0x06` for the low; the low test runs on `A`
>   *after* the high correction; `H` is always cleared at the end; `N` is never touched.
> * `DAA` is the one instruction in this lesson that reads both `H` and `N` **before**
>   writing `F`. Read them into locals at the top.
> * All seven opcodes are single-byte and cost **4 T-cycles**.

---

## 2. Your task — 55 min

Work in `gb/src/opcodes.c`.

1. **The four rotates** (15 min). `0x07`, `0x0F`, `0x17`, `0x1F`. Compute the new `A`
   and the carry-out in locals, then build `F` as `carry ? 0x10 : 0x00` — that is the
   whole flag byte, because `Z`, `N` and `H` are all zero.
2. **`CPL`/`SCF`/`CCF`** (15 min). `0x2F`, `0x37`, `0x3F`, each as "modify the bits in
   `F` that the table mentions".
3. **`DAA`** (20 min). `0x27`. Type the algorithm from §1 and then *trace your own
   implementation on paper* for `0x9A` and `0x3C` before running the tests. If your
   paper trace does not produce `0x00`/`0x90` and `0x42`/`0x00`, the bug is in front of
   you and not in the harness.
4. **Run the tests** (5 min):
   ```
   .\gb\build.cmd -Test m03
   ```
   `m03_rotates_a` and `m03_daa` green — which means **all ten `m03_*` tests** are
   green, and the whole 8-bit ALU is done.

---

## 3. Prove it — 10 min

```
.\gb\build.cmd -Test m03
```
```
  10 passed,    0 failed,    0 skipped   <- every m03 test, all green
ALL GREEN
```
Then the full regression sweep, because this lesson touched the same dispatcher as
everything else:

```
.\gb\build.cmd -Test m0
```
`m00`, `m01`, `m02` and `m03` all green; the first red test should now be in `m04_*`
(jumps and the stack — L12/L13).

The single most valuable check in this lesson: run `RLCA` with `A = 0x00` in a scratch
test and confirm `F` comes back `0x00` rather than `0x80`. That one assertion is the
difference between "my rotates work" and "my rotates work on the vectors I happened to
try".

---

## 4. If it fails

| Symptom | Cause | Find it |
| --- | --- | --- |
| `RLCA 0x00` returns `F = 0x80` | you computed `Z` from the result | `Z` is hard-zero for this family |
| `RLA`/`RRA` ignore the incoming carry | you masked `F` before reading `C` | read `C` into a local first |
| The rotate direction is mirrored | `"" << 1` on the wrong side for `RLCA` vs `RRCA` | `RL` rotates left, `RR` rotates right, and the mnemonic's last letter names A |
| `CPL` clears `Z` or `C` | you rebuilt `F` instead of editing it | only `N` and `H` change |
| `CCF` inverts `Z` instead of `C` | wrong bit | `C` is `0x10` |
| `DAA 0x9A` gives `0xA0` | you tested the low nibble using the original `A` | the test runs *after* the high correction |
| `DAA 0x9A` gives `0x06`-flavoured nonsense | you used `0x06` for the high-nibble correction | it is `0x60` |
| `DAA` after `SUB` adds instead of subtracting | you branched on `N == 1` the wrong way round | `N == 1` means the previous operation was a subtraction |
| `DAA` leaves `H` set | you forgot the final `H = 0` | the algorithm's last line |

---

## 5. Done when

- [ ] All ten `m03_*` tests are green
- [ ] `-Test m0` shows the first red test is now in `m04_*`
- [ ] You can state the flag rule for `RLCA` in one sentence, including the odd part
- [ ] You can explain why `DAA` needs the `N` flag at all
- [ ] Your paper trace of `DAA 0x9A` matched the implementation before you ran it
- [ ] Commit message like `feat(alu): rotates, CPL/SCF/CCF, DAA (L11)`

---

## 6. Optional, only if you have time

* `docs/02-cpu.md` §2.4 — the `DAA` section with the same worked checks, and a
  reminder of which later instructions (`ADD SP,e8`, L13) have their own flag shape.
* Implement `0x2F CPL`'s cousin for `(HL)`? There is none — `CPL` is A-only. Instead,
  spend the time on the `gb_disasm` mnemonic for today's seven opcodes so the tracer
  reads like assembly.

Next: **[L12 — Jumps and your first program](L12-jumps-and-your-first-program.md)**
