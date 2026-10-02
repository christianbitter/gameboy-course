# L06 — Loads, jumps, and loud failure

**Time** — theory ~20 min | coding ~60 min | verify ~10 min
**You will end with** — 16-bit immediates, a jump that moves the program counter,
memory addressed by a 16-bit constant, and a CPU that *faults correctly* on the 11
opcodes real hardware cannot execute. AHA: *I control PC, and my machine tells me
when it is asked for something impossible.*
**Tests that must go green** — `m02_ld_rr_d16`, `m02_pc_after_2byte`,
`m02_illegal_opcode_traps` (so all six `m02_*` are green after this)
**Depends on** — L05. You will edit `gb/src/opcodes.c` (and set `last_opcode_len`).

---

## 1. Read this — the whole theory for today

### Little endian, everywhere

A 16-bit immediate is stored **low byte first**. `01 34 12` is `LD BC,$1234`, not
`$3412`. This applies to every 16-bit immediate, every 16-bit address, and to how
the stack pushes values later. It is an 8080-family convention and the CPU reads the
bytes in that order because the bus is 8 bits wide.

### The opcodes for today

| Opcode | Instruction | Bytes | T-cycles |
| --- | --- | --- | --- |
| `0x01` | `LD BC,d16` | 3 | 12 |
| `0x11` | `LD DE,d16` | 3 | 12 |
| `0x21` | `LD HL,d16` | 3 | 12 |
| `0x31` | `LD SP,d16` | 3 | 12 |
| `0xC3` | `JP a16` | 3 | 16 |
| `0xEA` | `LD (a16),A` | 3 | 16 |
| `0xFA` | `LD A,(a16)` | 3 | 16 |

Each is "fetch the opcode, fetch 16 bits, then do one thing". `JP` overwrites `PC`
with what it fetched — that is all a jump is. `LD (a16),A` writes one byte to the
fetched address; it does **not** use a 16-bit bus access. (Real hardware performs a
16-bit *write* internally for timing reasons; that is L29, not today.)

### What "illegal opcode" means

Eleven base opcodes are undefined and lock real hardware up until you power-cycle:

```
D3  DB  DD  E3  E4  EB  EC  ED  F4  FC  FD
```

Every `0xCB`-prefixed opcode is defined; there are no other holes.

A game cannot legitimately reach one of these. If your emulator executes one, then
either a previous instruction already went wrong, or your dispatcher is decoding
wrong — which is exactly why the right response is a **fault**, not a shrug:

| Response | Why it is wrong |
| --- | --- |
| treat it as `NOP` | the machine silently continues with corrupted state and the failure surfaces elsewhere |
| `GB_UNIMPLEMENTED` | implies *you* have not written it yet; these can never be written |
| **`cpu_fatal()`** | correct: an impossible guest state, reported with a trace dump and `gb->fatal = 1` |

`cpu_fatal()` is provided in `cpu.c`. It prints `PC`, the opcode, all registers and
the last 12 trace lines, then sets `gb->fatal`. Every later `gb_step()` becomes a
no-op, and `main.c` exits with code 1.

> **What matters for the code you are about to write**
>
> * `LD rr,d16` = `cpu_fetch16()` then `cpu_set_bc/de/hl/sp(&gb->cpu, value)`.
>   The provided setters are `cpu_set_bc`, `cpu_set_de`, `cpu_set_hl`; **SP is a
>   plain `u16` field**, so assign it directly. There is no `cpu_set_sp`.
> * `JP a16` = `gb->cpu.pc = cpu_fetch16(gb);` — one line. `PC` is overwritten, not
>   added to. (Relative `JR` is L12.)
> * `LD (a16),A` = `u16 a = cpu_fetch16(gb); bus_write(gb, a, gb->cpu.a);`
>   `LD A,(a16)` = `gb->cpu.a = bus_read(gb, cpu_fetch16(gb));`
>   Order matters: fetch the address **before** touching A.
> * The 11 illegal opcodes must call `cpu_fatal(gb, "illegal opcode %02X", op)`.
>   `m02_illegal_opcode_traps` executes `0xD3` and then asserts `gb->fatal`.
> * Your dispatcher must also set `gb->cpu.last_opcode_len` to 1, 2 or 3 for the
>   instruction it ran. The cleanest place is wherever you already know the length
>   (the table's `bytes` field, or each case).

---

## 2. Your task — 60 min

Work in `gb/src/opcodes.c`. Keep the `GB_UNIMPLEMENTED` default case from L05.

1. **`LD rr,d16`** (15 min). Four cases. For `0x31` remember `SP` is a bare `u16`:
   `gb->cpu.sp = cpu_fetch16(gb);` Return 12.
2. **`JP a16`** (5 min). `gb->cpu.pc = cpu_fetch16(gb);` Return 16.
3. **`LD (a16),A` and `LD A,(a16)`** (15 min). Fetch the address into a local, then
   read or write through the bus. Return 16 each.
4. **The illegal list** (15 min). Write them as a small helper:
   ```c
   static bool is_illegal(u8 op) {
       switch (op) {
       case 0xD3: case 0xDB: case 0xDD: case 0xE3: case 0xE4: case 0xEB:
       case 0xEC: case 0xED: case 0xF4: case 0xFC: case 0xFD:
           return true;
       default: return false;
       }
   }
   ```
   Call it at the top of `op_execute`: `if (is_illegal(op)) { cpu_fatal(gb, "illegal
   opcode %02X at PC=%04X", op, pc); return 4; }`
   The returned 4 never matters (the machine is stopped), but your compiler will want
   a value.
5. **Instruction length** (10 min). Make `last_opcode_len` correct for everything you
   have implemented: 1 for `NOP`, 2 for `LD r,d8`, 3 for today's seven opcodes. If
   your dispatcher is a table, this is free — the table already stores `bytes`.
6. **Run the tests** (5 min):
   ```
   .\gb\build.cmd -Test m02
   ```
   All six green.

---

## 3. Prove it — 10 min

```
.\gb\build.cmd -Test m02
```
```
   6 passed,    0 failed,    0 skipped  (74 tests registered)
ALL GREEN
```
Then the regression sweep:
```
.\gb\build.cmd -Test m0
```
`m00_*`, `m01_*`, `m02_*` green; `m03_*` onward red with `UNIMPLEMENTED`. For bonus
evidence that the fault path really stops the machine, run the emulator on a ROM made
of `0xD3` bytes in a scratch test and watch it print the trace and exit 1.

---

## 4. If it fails

| Symptom | Cause | Find it |
| --- | --- | --- |
| `m02_ld_rr_d16`: `BC` is `$3412` | you built the 16-bit value high-byte-first | low byte first, always |
| `m02_ld_rr_d16` passes for `BC` but `SP` is 0 | you called a `cpu_set_sp`-shaped helper that does not exist, or wrote to `cpu.pc` | `SP` is a plain `u16` field |
| `m02_pc_after_2byte`: `PC` is `0x0103` after `JP` | you fetched the address but never assigned it to `PC` | the assignment *is* the jump |
| `m02_pc_after_2byte`: the `(a16)` store writes to the wrong place | you passed the *low byte* of the address | `bus_write` takes the full `u16` |
| `m02_illegal_opcode_traps` reports `UNIMPLEMENTED` instead of `fatal` | `0xD3` hit your default case first | check `is_illegal` before the dispatcher, not inside it |
| `m02_illegal_opcode_traps` crashes the whole run instead of failing one test | you called `abort()` yourself | `cpu_fatal` sets a flag; never `abort()` for a guest fault |
| A test that used to pass now fails on `last_opcode_len` | you set the length only in the new cases | set it in one place for every path |

---

## 5. Done when

- [ ] All six `m02_*` tests are green
- [ ] `-Test m0` shows `m00_*`/`m01_*`/`m02_*` green and nothing else newly broken
- [ ] You can list the 11 illegal opcodes from memory (or explain why you keep them in a helper)
- [ ] You can explain the difference between `GB_UNIMPLEMENTED` and `cpu_fatal` in one sentence each
- [ ] A scratch test with a `0xD3` program prints a register dump and sets `fatal`
- [ ] Commit message like `feat(cpu): 16-bit loads, JP, and illegal-opcode faults (L06)`

---

## 6. Optional, only if you have time

* `docs/02-cpu.md` §2.3 — why the table records both a "taken" and "not taken" cost
  for conditional instructions, which you will need in L12.
* Add `0x08 LD (a16),SP` now if you want a head start; it is otherwise L13's.

Next: **[L07 — The load matrix](L07-the-load-matrix.md)**
