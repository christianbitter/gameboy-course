# 02 — The CPU: Sharp LR35902

## 2.0 What you are implementing

The Game Boy's CPU is a custom 8-bit chip usually written **LR35902**. It is not
an 8080 and not a Z80, but it sits between them:

* 8080-ish: the register layout, flags, `PUSH`/`POP`, `RST`, `DAA`, `DAD`-style 16-bit add.
* Z80-ish: `JR`, the `CB` prefix (bit ops and shifts), `LD (HL+),A`, `ADD SP,e8`, `LDH`.
* Neither: no `IX`/`IY`, no `IN`/`OUT`, no `EX`, no alternate register set, no `IN`/`OUT` I/O ports — everything is memory-mapped.

Two facts drive the whole design:

1. **Every instruction is 1, 2 or 3 bytes, and the first byte is always the opcode.**
   There is no variable-length prefix soup like x86. Decoding is: read a byte,
   look it up.
2. **Everything is measured in T-cycles** (ticks of the 4.194304 MHz master clock).
   4 T-cycles = 1 M-cycle = the time to fetch one byte. Hardware docs give
   instruction costs in M-cycles; multiply by 4.

## 2.1 Registers

| Reg | Width | Notes |
| --- | --- | --- |
| A | 8 | accumulator |
| F | 8 | flags; only the top 4 bits exist — see below |
| B C D E H L | 8 each | general purpose, paired for 16-bit use |
| SP | 16 | stack pointer |
| PC | 16 | program counter, points at the *next* byte to fetch |
| AF BC DE HL | 16 | virtual pairs: `A<<8|F`, `B<<8|C`, ... |

Flag layout (F):

| bit | 7 | 6 | 5 | 4 | 3..0 |
| --- | --- | --- | --- | --- | --- |
| name | Z | N | H | C | always 0 |
| value | 0x80 | 0x40 | 0x20 | 0x10 | 0x00 |

* **Z** zero, **N** subtract (BCD aid), **H** half carry (carry out of bit 3),
  **C** carry (carry out of bit 7).
* The low nibble of F **does not exist**. It reads 0. You must mask on write and
  on `POP AF`, `LD F` is impossible (F is never a direct operand), and any
  `uint8_t a, f;` pair must be combined as `(a << 8) | (f & 0xF0)`.

### Storage decision (pick one and be consistent)

```
Option A (recommended):  u8 a,f,b,c,d,e,h,l;  u16 sp,pc;
                         inline u16 hl(const cpu_t*) { return c->h<<8 | c->l; }
Option B:                union { struct { u8 l,h; } r; u16 v; } hl;
```

Do **not** use `union { struct {u8 f,a;} ; u16 af; }` together with direct field
access. Byte order inside a union is implementation-defined, you will get
`AF` reversed on some build, and you will spend an evening on it. Option A costs
nothing and is impossible to get wrong.

## 2.2 Instruction classes

| Class | Opcode range | What it does |
| --- | --- | --- |
| 8-bit load | `0x40-0x7F`, `0x06/0E/16/1E/26/2E/3E`, `0x36`, `0x0A/1A/2A/3A`, `0x02/12/22/32`, `0xE0/E2/F0/F2`, `0xEA/FA` | copies |
| 16-bit load | `0x01/11/21/31`, `0x08`, `0xF8`, `0xF9`, `0xF1/F5` etc. | immediate/bulk moves |
| 8-bit ALU | `0x80-0xBF`, `0xC6/CE/D6/DE/E6/EE/F6/FE` | arithmetic, logic, compare |
| 16-bit ALU | `0x03/13/23/33/0B/1B/2B/3B`, `0x09/19/29/39`, `0xE8` | inc/dec pairs, `ADD HL,rr`, `ADD SP,e8` |
| Rotate/shift A | `0x07/0F/17/1F` | fast rotates, always clear Z |
| Rotate/shift r | `0xCB 0x00-0x3F` | full rotates/shifts with Z set |
| Bit ops | `0xCB 0x40-0xFF` | `BIT`, `RES`, `SET` |
| Jumps | `0xC2/C3/CA/D2/DA`, `0x18/20/28/30/38`, `0xE9` | absolute, relative, indirect |
| Calls/returns | `0xC4/CC/CD/D4/DC`, `0xC0/C8/C9/D0/D8/D9`, `0xC7..0xFF` steps of 8 | stack + `RST` vectors |
| Stack | `0xC1/C5/D1/D5/E1/E5/F1/F5` | `PUSH`/`POP` |
| Misc | `0x00`, `0x10`, `0x27`, `0x2F`, `0x37`, `0x3F`, `0xF3`, `0xFB` | `NOP STOP DAA CPL SCF CCF DI EI` |
| Illegal | 11 opcodes, see `reference/cheatsheet-opcode-map.md` | lock up on real hardware |

## 2.3 Cycle accounting — decide this first

There are three places cycles can be counted, and mixing them is misery:

| Model | How | Verdict |
| --- | --- | --- |
| Instruction total | table lookup, `cpu_step()` returns the whole cost | start here |
| Per memory access | `bus_tick(4)` inside every `bus_read`/`bus_write` | refine here in M09 |
| Per fetch + internal cycle | full model, incl. dummy reads | only if a test fails |

Start with: a per-opcode table of total T-cycles, `cpu_step()` returns it,
`gb_step()` calls `bus_tick(tcycles)` once.

Two things that bite at this granularity:

* **Conditional instructions have two costs.** `JR NZ,e8` is 12 T taken, 8 T not
  taken; `CALL cc` is 24/12; `RET cc` is 20/8; `JP cc` is 16/12. Your table needs
  both columns.
* **`0xCB` is a prefix, not an instruction.** Decide once whether the `0xCB`
  fetch is part of the sub-opcode's cost. The conventional totals are 8 T for a
  `CB` register operation, 16 T for `CB ... (HL)`, 12 T for `BIT (HL)`. If your
  dispatcher counts the prefix as an extra 4 T, subtract that in the sub-table.
  Be internally consistent and let `mem_timing` arbitrate.
* **Unimplemented opcodes must abort loudly.** `GB_UNIMPLEMENTED("opcode %02X at %04X", op, pc)`
  printing a full register dump is worth more than any debugger you will write
  later. Silent NOP-on-unknown corrupts state invisibly.

## 2.4 Flags — the full rules

This is the single densest source of beginner bugs. Learn the shape of the table,
then verify every cell with a data-driven test.

| Operation | Z | N | H | C |
| --- | --- | --- | --- | --- |
| `ADD A,n` | result == 0 | 0 | `(a&0xF)+(n&0xF) > 0xF` | `a+n > 0xFF` |
| `ADC A,n` | result == 0 | 0 | low-nibble sum + carry > 0xF | `a+n+carry > 0xFF` |
| `SUB n` | result == 0 | 1 | `(a&0xF) < (n&0xF)` | `a < n` |
| `SBC A,n` | result == 0 | 1 | `(a&0xF) < (n&0xF)+carry` | `a < n+carry` |
| `AND n` | result == 0 | 0 | **1** | 0 |
| `XOR n` / `OR n` | result == 0 | 0 | 0 | 0 |
| `CP n` | result == 0 | 1 | as `SUB` | as `SUB` |
| `INC r` | result == 0 | 0 | `(r&0xF) == 0xF` | **unchanged** |
| `DEC r` | result == 0 | 1 | `(r&0xF) == 0x0` | **unchanged** |
| `ADD HL,rr` | unchanged | 0 | `(hl&0xFFF)+(rr&0xFFF) > 0xFFF` | `hl+rr > 0xFFFF` |
| `INC/DEC rr` | unchanged | unchanged | unchanged | unchanged |
| `ADD SP,e8` | **0** | 0 | carry out of bit 3 of `(SP&0xFF) + (u8)e8` | carry out of bit 7 of the same |
| `LD HL,SP+e8` | **0** | 0 | same as above | same as above |
| `RLCA/RRCA/RLA/RRA` | **0** | 0 | 0 | bit rotated out |
| `CB` rotates/shifts | result == 0 | 0 | 0 | bit shifted out |
| `SWAP` | result == 0 | 0 | 0 | 0 |
| `BIT b,r` | `!(r & 1<<b)` | 0 | **1** | unchanged |
| `SLA/SRL/SRA` | result == 0 | 0 | 0 | bit shifted out |
| `CPL` | unchanged | 1 | 1 | unchanged |
| `SCF` | unchanged | 0 | 0 | 1 |
| `CCF` | unchanged | 0 | 0 | `!C` |
| `DAA` | see below | unchanged | 0 | see below |
| loads, jumps, stack, `NOP` | unchanged | | | |

High-value details people miss:

* `CP` is `SUB` for flags but **must not write A**.
* `AND` sets **H = 1**. It is the only logic op that does. This is not a typo.
* `INC`/`DEC` do **not** touch C.
* `ADD HL,rr` uses bit 11 for H, not bit 3.
* `ADD SP,e8` / `LD HL,SP+e8` are the *only* place where the flags come from an
  8-bit addition whose operands are the two low bytes. `e8` is signed for the
  *result*, unsigned for the *flags*: `(SP&0xFF) + e8` as unsigned bytes.
* `BIT` sets H = 1 (yes, even `BIT` where you would expect no flag write).

### Arithmetic implementation notes

Work in `int` (or `unsigned`), never in `u8`, then truncate:

```c
int r = a + n;                 /* 0..510, carry is r > 0xFF */
int half = (a & 0xF) + (n & 0xF);   /* half carry is half > 0xF */
```

For `SBC`, a clean trick is to add the complemented carry:
`int r = a - n - carry;` with borrow detection `r < 0` (then store `r & 0xFF`),
and half-borrow `(a & 0xF) - (n & 0xF) - carry < 0`.

### DAA in detail

`DAA` corrects the accumulator after a BCD (binary-coded decimal) addition or
subtraction. Each nibble is a decimal digit 0-9. The **N** flag tells you which
correction to apply. The classic bug is writing `0x06` for the high nibble; the
high-nibble correction is `0x60`:

```
if N == 0:
    if C == 1 or A > 0x99:  A += 0x60;  C = 1
    if H == 1 or (A & 0x0F) > 0x09:  A += 0x06
else:
    if C == 1:  A -= 0x60
    if H == 1:  A -= 0x06

A &= 0xFF
Z = (A == 0)
H = 0
/* N unchanged, C as set above */
```

Worked checks to include in your test table:

| A | N | H | C | expected A | expected flags |
| --- | --- | --- | --- | --- | --- |
| 0x9A | 0 | 0 | 0 | 0x00 | Z=1, C=1 |
| 0x3C | 0 | 0 | 0 | 0x42 | Z=0, C=0 |
| 0x2D | 1 | 1 | 0 | 0x27 | Z=0, C=0 |
| 0xFF | 1 | 1 | 1 | 0x99 | Z=0, C=1 |
| 0x00 | 0 | 1 | 0 | 0x06 | Z=0, C=0 |

Note the second low-nibble test uses A **after** the high-nibble correction —
that is what the hardware does, and it is what makes `0x9A -> 0x00` come out right.

## 2.5 Operand fetching and PC semantics

```
u8 fetch8(gb)   { return bus_read(gb, gb->cpu.pc++); }
u16 fetch16(gb) { u16 lo = fetch8(gb); u16 hi = fetch8(gb); return lo | hi<<8; }
```

* All 16-bit immediates and addresses are **little endian**: low byte first.
* `PC` is incremented by the fetch itself, so inside an instruction `PC` already
  points past the bytes you read. Anything that writes `PC` (`JP`, `CALL`, `RET`,
  interrupt dispatch) overwrites it wholesale.
* `LD (a16),SP` writes `SP & 0xFF` to `a16` and `SP >> 8` to `a16+1`.
* `LD (HL+),A` / `LD A,(HL+)`: the memory access happens **first**, then HL is
  incremented; `LD (HL-),A` decrements after. Order matters when an interrupt or
  a read of `(HL)` is interleaved; write it in the specified order.

### The `(HL)` operand

The `r8` operand space is `B C D E H L (HL) A` — index 0..7. Index 6 means
"read/write `bus_read(HL)`", which costs 4 extra T-cycles:

```
u8   cpu_read_r8 (gb, idx);   /* idx 6 -> bus_read(gb, HL)      */
void cpu_write_r8(gb, idx, v);/* idx 6 -> bus_write(gb, HL, v)  */
```

Writing these two helpers early collapses `0x40-0x7F` (64 opcodes) and
`0x80-0xBF` (64 opcodes) into two loops of 8 in your table, and it is the
difference between 512 hand-written functions and about 40.

## 2.6 Interrupts

| Reg | Addr | Meaning |
| --- | --- | --- |
| IF | `0xFF0F` | requested flags, bits 0-4 |
| IE | `0xFFFF` | enable mask, bits 0-4 |
| IME | — | master enable, not memory-mapped |

| bit | source | vector |
| --- | --- | --- |
| 0 | VBlank | `0x0040` |
| 1 | LCD STAT | `0x0048` |
| 2 | Timer | `0x0050` |
| 3 | Serial | `0x0058` |
| 4 | Joypad | `0x0060` |

Dispatch, checked **before** fetching the next opcode:

```
pending = IE & IF & 0x1F
if (IME && pending):
    bit  = index of lowest set bit of pending    /* VBlank wins ties */
    IME  = 0
    IF  &= ~(1 << bit)
    SP  -= 2
    write16(SP, PC)
    PC   = 0x40 + bit * 8
    return 20 T-cycles
```

* Priority is by **lowest bit number**, not by arrival order.
* `PUSH PC` order: write `PC >> 8` at `SP-1` and `PC & 0xFF` at `SP-2`, i.e.
  `bus_write(--SP, hi); bus_write(--SP, lo);`.
* **`EI` has a one-instruction delay.** Hardware enables IME after the instruction
  *following* `EI` executes. Implement `ime_pending`; `EI` sets it, and it
  promotes to `IME` when the next instruction finishes. Without this, `EI; RETI`
  sequences and the `HALT` wake path go subtly wrong.
* `DI` clears IME immediately. `RETI` pops the PC **and** sets `IME = 1`
  immediately (no delay).
* `HALT` (0x76) stops the CPU until `IE & IF != 0`. If an interrupt is already
  pending while `IME == 0`, the CPU does not stay halted — and on real hardware
  this produces the **halt bug**: the byte after `HALT` is read twice (the PC
  fails to increment once). Optional to implement, tested by blargg `halt_bug`.
* `STOP` (0x10) is followed by a padding byte `0x00`; treat it as a 2-byte
  instruction (and later as the CGB speed-switch trigger).

## 2.7 How to structure the opcode table

Two acceptable designs; pick one, both hide the same information:

```
A)  typedef struct {
        const char *mnemonic;
        u8 bytes;          /* 1, 2 or 3 */
        u8 cycles;         /* T-cycles when not taken / unconditionally */
        u8 cycles_taken;   /* T-cycles when a condition is true, else 0 */
        void (*fn)(gb_t *);
    } opcode_t;
    extern const opcode_t op_table[256];
    extern const opcode_t cb_table[256];

B)  a single switch (op) { case 0x00: ... } with the count baked into a
    side table.
```

Function-pointer tables make the disassembler and the tracer trivial, because
the table already contains the mnemonic and the length. That is a strong
argument for (A): your `gb_disasm()` (M11) becomes a table read, and any hole in
the table shows up as `???`.

Add a self-check in M02: assert that every one of the 256 entries has a non-NULL
function or is one of the 11 documented illegal opcodes.

## 2.8 "My CPU tests fail" checklist

Check these in order, before you touch anything clever:

1. F is not masked to `0xF0` when read or on `POP AF`.
2. `H` on `SUB`/`SBC` computed as a carry instead of a borrow.
3. `CP` writing to A.
4. `ADC`/`SBC` ignoring the incoming carry in the H/C computations.
5. `INC`/`DEC` clobbering C.
6. `ADD HL,rr` setting H from bit 3 instead of bit 11.
7. 16-bit accessors not masking to `0xFFFF` (`hl = (h<<8)|l | 0xFF00` type bugs).
8. `(HL)` operand writing through the wrong register (e.g. after a swap for `(HL-)`).
9. PC off by one after 2- and 3-byte instructions, or `fetch16` reading high byte first.
10. `EI` enabling IME immediately instead of after the next instruction.
11. Interrupt dispatch pushing PC in the wrong order, or not clearing the IF bit.
12. Peripheral code that forgets to set its IF bit (timer overflow, VBlank, joypad).
13. `DAA` using `0x06` for the high-nibble correction.

Every one of those has a named test in `gb/tests/`. Run them; do not debug by
staring.

## 2.9 Where the work is

| Milestone | What you implement here |
| --- | --- |
| M02 | registers, fetch, decode skeleton, `NOP`/`LD`/`JP`, loud abort |
| M03 | the load matrix, the whole 8-bit ALU, rotates, `DAA` |
| M04 | control flow, stack, the whole `CB` block |
| M05 | interrupts, `HALT`, timers, serial |
| M09 | access-level cycle accuracy |

## Sources

* **Pan Docs:** *CPU Registers and Flags* (the A/F/B/C/D/E/H/L file and F's low
  nibble), *CPU Instruction Set* (per-opcode flag columns and the illegal list),
  *Interrupts* (IME semantics and the `EI` delay), *CPU Comparison with Z80* (what
  not to copy).
* **gbctr** (Game Boy: Complete Technical Reference), CPU chapter: per-instruction
  M-cycle timing diagrams.
* Cycle counts in this document were cross-checked against the two vendored
  machine-readable tables in [../reference/tables/](../reference/tables) — gbctr's
  `opcodes.toml` (MIT) and gb-opcodes' `Opcodes.json` (CC0). The `CB`-prefix
  convention is stated explicitly in
  [../reference/cheatsheet-flags-and-timing.md](../reference/cheatsheet-flags-and-timing.md).
* Oracles: blargg's `cpu_instrs`, `instr_timing`, `halt_bug`; mooneye's
  `acceptance/` suite. Full annotations:
  [../reference/external-references.md](../reference/external-references.md).

Next: **[03-memory-and-cartridge.md](03-memory-and-cartridge.md)** — the bus, the
memory map, and how a cartridge lies to the CPU about how much ROM it has.
