# L05 — The CPU wakes up

**Time** — theory ~25 min | coding ~55 min | verify ~10 min
**You will end with** — a program counter that advances, an opcode dispatcher you
control, and a machine that aborts loudly on anything you have not implemented.
AHA: *the tiny machine runs.*
**Tests that must go green** — `m02_nop_advances_pc`, `m02_step_cycles_nop`,
`m02_ld_r_d8`
**Depends on** — L03 (the bus answers reads). You will edit `gb/src/cpu.c` and
`gb/src/opcodes.c`.

---

## 1. Read this — the whole theory for today

### The register file

| Register | Width | Role |
| --- | --- | --- |
| A | 8 | accumulator — most arithmetic happens here |
| F | 8 | flags. **Only the top 4 bits exist**; the low nibble always reads 0 |
| B C D E H L | 8 each | general purpose; paired for 16-bit use |
| SP | 16 | stack pointer |
| PC | 16 | program counter: the address of the **next** byte to fetch |

| F bit | 7 | 6 | 5 | 4 | 3..0 |
| --- | --- | --- | --- | --- | --- |
| name | Z | N | H | C | — |
| value | `0x80` | `0x40` | `0x20` | `0x10` | 0 |

Sixteen-bit pairs are virtual: `AF = A<<8 | F`, `BC`, `DE`, `HL`. The struct in
`gb/include/gb/cpu.h` stores the eight 8-bit registers separately and provides
`cpu_af()`, `cpu_set_hl()` and friends inline. Use them. Do **not** add a
`union { struct {u8 f,a;}; u16 af; }` on top: byte order inside a union is
implementation-defined and you will get `AF` reversed on some build.

### Instructions are 1, 2 or 3 bytes

The first byte is always the opcode. No prefixes except `0xCB` (L14). Decoding is
therefore "read a byte, look it up", and operand bytes follow immediately after.

### Two units of time

| Unit | Meaning |
| --- | --- |
| **T-cycle** | one tick of the 4.194304 MHz master clock — the unit of all timing |
| **M-cycle** | 4 T-cycles — the time to fetch one byte |

`NOP` is 1 M-cycle = **4 T**. `LD A,d8` is 2 M-cycles = **8 T** (fetch the opcode,
fetch the operand). Every opcode's cost is written in M-cycles in every reference
table, so multiply by 4. `cpu_step()` returns **T-cycles**.

### Fetching, and what PC means

```c
u8 fetch8(gb)  { return bus_read(gb, gb->cpu.pc++); }        /* post-increment */
u16 fetch16(gb) { u16 lo = fetch8(gb); u16 hi = fetch8(gb);  /* low byte first */
                  return lo | (hi << 8); }
```

`PC` always points at the next byte to fetch. This is why instructions are short to
write: `LD A,d8` is "fetch the opcode, then `cpu_fetch8()`". Anything that writes
`PC` (`JP`, `CALL`, `RET`, an interrupt) overwrites it wholesale, and that is also
short.

### The two failure kinds — do not confuse them

| Mechanism | Meaning | Behaviour |
| --- | --- | --- |
| `GB_UNIMPLEMENTED("opcode %02X", op)` | *you have not written this yet* — a bug in your emulator | prints file:line and aborts (under the test harness, marks that test failed) |
| `cpu_fatal(gb, "...")` | *the guest did something impossible* — a real hardware fault | sets `gb->fatal`, dumps the trace, and the machine stops |

Never let an unknown opcode fall through to `NOP`. A silently skipped instruction
corrupts one register, then one jump, and the failure appears four minutes later in
an unrelated place.

### The dispatch design (decide once, today)

Either works:

* **A table**: `static const opcode_t table[256]` with `{ mnemonic, bytes, cycles,
  cycles_taken, fn }`. The mnemonic in the table is what makes L33's disassembler a
  five-line function, and a hole in the table is visible as `???`.
* **A switch**: compact to start, easy to step through in gdb, and you add the cycle
  counts in a parallel array.

Pick one and write it in `NOTES.md`. You are not locked in — `op_execute()` hides
the choice from `cpu_step()`.

### The division of labour you are given

```
gb_step()      provided:  cycles = cpu_step(gb);  bus_tick(gb, cycles);
cpu_step()     yours:     one instruction or one interrupt dispatch
op_execute()   yours:     what one opcode does, returns its T-cycles
```

> **What matters for the code you are about to write**
>
> * `cpu_fetch8` does `bus_read(gb, gb->cpu.pc++)`. The post-increment **is** the
>   PC semantics; there is no separate PC advance anywhere.
> * `cpu_fetch16` reads **low byte first**. Reversing it is the single most common
>   bug in this lesson, and `m02_ld_rr_d16` exists to catch it (that is L06).
> * `cpu_step` must set `gb->cpu.last_opcode` and `gb->cpu.last_opcode_len` (1, 2 or
>   3). The tracer reads both; if you skip them the trace is useless and
>   `m02_nop_advances_pc` fails on `last_opcode_len`.
> * `cpu_step` returns **T-cycles**: `NOP` -> 4, `LD r,d8` -> 8.
> * Leave the `ime_pending` promotion, the interrupt dispatch and the `HALT` check
>   as empty hooks today — they are L15. Do not implement them "while you are here".
> * The default case of your dispatcher must be `GB_UNIMPLEMENTED("opcode %02X at
>   PC=%04X", op, pc)`, with the PC of the opcode, not the post-fetch PC.

---

## 2. Your task — 55 min

1. **Read the contract** (10 min). `gb/include/gb/cpu.h` (the struct, the accessors,
   the macro flags), `gb/src/cpu.c` (your stubs), `gb/src/gb.c`'s `gb_step()`, and
   `gb/tests/harness.h`'s `t_exec()` so you know exactly how you are being called.
2. **Fetch** (10 min). Implement `cpu_fetch8` and `cpu_fetch16` in `cpu.c`.
3. **The r8 accessor pair** (20 min). Implement `cpu_read_r8(gb, idx)` and
   `cpu_write_r8(gb, idx, v)` where `idx` is 0..7 = `B C D E H L (HL) A`.
   Index 6 goes through the bus at `HL` — that single special case is what turns 128
   opcodes into two loops in L07, so get it right now.
4. **`cpu_step`** (10 min):
   ```
   if (gb->fatal) return 0;                 /* the machine has already stopped */
   /* TODO(L15): promote ime_pending; dispatch a pending interrupt; handle HALT */
   u16 pc = gb->cpu.pc;
   gb->cpu.last_opcode = cpu_fetch8(gb);
   /* TODO(L06): opcodes know their own length; set last_opcode_len there */
   gb->cpu.last_opcode_len = 1;
   gb->cpu.instruction_count++;
   return op_execute(gb, gb->cpu.last_opcode);
   ```
   Keep `pc` only if you use it for the error message; the tracer gets it from
   `gb_step()`.

   Do not commit `last_opcode_len = 1` as a permanent answer: in L06 your dispatcher
   sets the real length. It is here so that NOP can go green today.
5. **`op_execute`** (15 min). In `opcodes.c`:
   * `0x00` `NOP` -> 4
   * `0x06 / 0x0E / 0x16 / 0x1E / 0x26 / 0x2E / 0x3E` -> `LD r,d8`:
     `cpu_write_r8(gb, index, cpu_fetch8(gb))`, 8 T. The index is `(op >> 3) & 7`.
   * everything else -> `GB_UNIMPLEMENTED("opcode %02X at PC=%04X", op, pc)`.
     *You need the opcode's own address for that message; either pass it in or read
     it back as `gb->cpu.pc - 1` before you fetch the operand.*
6. **Run the tests** (5 min):
   ```
   .\gb\build.cmd -Test m02
   ```
   Expect `m02_nop_advances_pc`, `m02_step_cycles_nop`, `m02_ld_r_d8` **green**, and
   the other three `m02_*` red — they are L06.
7. **See the tracer work** (5 min). In a scratch copy of the tests, or via
   `gb->trace.enabled = true`, run a few `cpu_step`s and print
   `gb_trace_print_last(gb, stdout, 8)`. You should see `0100 00 00 00` with
   registers. That output is your debugging instrument for the rest of the course.

---

## 3. Prove it — 10 min

```
.\gb\build.cmd -Test m02
```
```
[       OK ] m02_nop_advances_pc
[  FAILED  ] m02_ld_rr_d16          <- L06
[       OK ] m02_step_cycles_nop
[       OK ] m02_ld_r_d8
[  FAILED  ] m02_pc_after_2byte     <- L06
[  FAILED  ] m02_illegal_opcode_traps <- L06
```
Three green, three red, and the ones that are red name a feature you have not
written yet. Then:

```
.\gb\build.cmd -Test m0
```
`m00_*` and `m01_*` must still be green; `m03_*` onward red.

---

## 4. If it fails

| Symptom | Cause | Find it |
| --- | --- | --- |
| `m02_nop_advances_pc` expects PC `0x0101`, sees `0x0100` | you advanced PC somewhere other than in `cpu_fetch8` | the post-increment **is** the advance |
| `last_opcode_len` is 0 | you never set it in `cpu_step` | the tracer and this test both read it |
| `m02_step_cycles_nop` gets 0 | `NOP` returned 0, or your default case swallowed it | print the returned value |
| `m02_ld_r_d8` gets the wrong register | index order is `B C D E H L (HL) A`, not `A B C ...` | the `(op >> 3) & 7` index for `0x3E` must be 7 |
| The run aborts at the first unimplemented opcode instead of reporting a failure | you called `cpu_fatal` for a missing feature | missing feature = `GB_UNIMPLEMENTED`; impossible guest state = `cpu_fatal` |
| Compile error about `cpu_t` vs `gb_t` | `cpu_step` takes `gb_t *`, the setter helpers take `cpu_t *` | `cpu_set_af(&gb->cpu, ...)` |

---

## 5. Done when

- [ ] Three `m02_*` tests green, three red, and you can name which feature each red one wants
- [ ] `-Test m0` shows no regression in `m00_*`/`m01_*`
- [ ] A scratch run prints a tracer line for `NOP` with the right PC
- [ ] You can write `cpu_fetch16` from memory, including byte order
- [ ] You can explain why an unknown opcode aborts instead of acting as `NOP`
- [ ] `NOTES.md` records which dispatch design you chose and why
- [ ] Commit message like `feat(cpu): fetch/decode loop, NOP and LD r,d8 (L05)`

---

## 6. Optional, only if you have time

* `docs/02-cpu.md` §2.1, §2.3, §2.5 — the same material plus the cycle-count table
  conventions (including how `0xCB` timing is counted).
* Add a `gb_disasm()` case for your two opcodes so the tracer prints `NOP` and
  `LD A,$42` instead of hex bytes. Fifteen minutes, and it pays off every day after.

Next: **[L06 — Loads, jumps, and loud failure](L06-loads-jumps-and-loud-failure.md)**
