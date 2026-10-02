# L33 — The disassembler and the debugger

**Time** — theory ~25 min | coding ~55 min | verify ~10 min
**You will end with** — a trace you can *read*, and the workflow that finds a bug by
reading it. AHA: *I have been debugging hex for eleven lessons; this is how people actually
do it.*
**Tests that must go green** — `m11_disassembler_covers_all_ops`,
`m11_trace_ring_keeps_last_n`
**Depends on** — L01 (the tracer), L06 (`cpu_fatal` and the trace dump). You will implement
the body of `gb_disasm` in `gb/src/debug.c`.

---

## 1. Read this — the whole theory for today

### Why a disassembler is not a luxury

`--trace` has existed since L01 and prints:

```
...  PC=0102  A=00 F=B0 ...  opcode bytes ...
```

That was enough while you were writing 30 instructions. It stops being enough the moment a
game jumps somewhere wrong, because the question is never "what bytes executed" but "what
did they *mean*, and where did the first wrong branch happen". A disassembly turns

```
C3 50 01  |  CD 00 40  |  F5 3E 03  ...
```

into

```
JP $0150        ; <- why here?
CALL $4000
PUSH AF
LD A,$03
```

and the answer becomes visible. The tracer already calls `gb_disasm` for every entry
(`debug.c` writes the disassembly into `trace_entry_t.text`), so today you make the tracer
readable.

### The shape of the job: 245 + 256 opcodes

There are 256 base opcodes, 11 of which are illegal, and 256 `CB`-prefixed ones: 501
instructions to decode. Typing them out is not the interesting part and is not the point —
the point is the **regularity**, which you can exploit exactly as you did in L07, L08 and
L14.

**The base page has blocks:**

| Range | Shape | How to decode |
| --- | --- | --- |
| `0x40-0x7F` (except `0x76`) | `LD r,r'` | an 8x8 matrix: `dst = (op >> 3) & 7`, `src = op & 7`, both indexing `{B,C,D,E,H,L,(HL),A}` |
| `0x00, 0x08, 0x10, ... 0x38` | `LD r,d16` | eight of them, one per register pair |
| `0x06 + 8n` | `LD r,d8` | eight of them, immediate |
| `0x04 + 8n`, `0x05 + 8n` | `INC r` / `DEC r` | the same 8-register order |
| `0x80-0xBF` | the ALU block | `op >> 3` gives the operation, `op & 7` the source register |
| `0xC6, 0xCE, 0xD6, 0xDE, 0xE6, 0xEE, 0xF6, 0xFE` | the eight ALU immediates | `(op & 0xC7) == 0xC6` selects the family — the same trick the ALU implementation and its test fixture use |
| `0xC0-0xFF` | `RET/JP/CALL` conditions | `(op >> 3) & 7` gives the condition, and the four `00/08/10/18` variants give the shape |
| `0x00-0x3F` | the rest | a jump table is fine here; there are only ~40 |

**The CB page is eight groups of eight:**

| Range | Group | Operand |
| --- | --- | --- |
| `0x00-0x3F` | `RLC RRC RL RR SLA SRA SWAP SRL` | `group = op >> 3`, `reg = op & 7` |
| `0x40-0x7F` | `BIT b,r` | `bit = (op >> 3) & 7`, `reg = op & 7` |
| `0x80-0xBF` | `RES b,r` | same |
| `0xC0-0xFF` | `SET b,r` | same |

So the CB page is one 8-entry table plus the `(bit, reg)` split — about twenty lines total.

### Operand bytes come from the ROM, not the bus

`gb_disasm` must read the following bytes to print them:

```c
C3 50 01        -> "JP $0150"     ; needs 2 operand bytes
7E              -> "LD A,(HL)"    ; needs none
```

Read them from **`gb->cart.rom` directly**, never through `bus_read`. Two reasons, both
real:

1. the tracer calls `gb_disasm` for every instruction, and a `bus_read` would tick the bus
   and corrupt the timing you are trying to observe;
2. `bus_read(0x4000-0x7FFF)` returns the *currently banked* byte, which is right for
   execution but wrong for a disassembler looking at a fixed address.

`debug.c` already documents this in a comment above `gb_disasm`. It also means you must
bounds-check: a PC in the last two bytes of the ROM must not read past the end.

### Operand formatting: pick conventions and stick to them

There is no standard. What matters is that a human can read it and that the tests'
requirement is satisfied: **every legal opcode must start with a mnemonic letter**, and the
six spot-checked ones must contain `NOP`, `HALT`, `JP`, `RLC`, `BIT`, `SET`. A working set:

| Operand | Suggested spelling |
| --- | --- |
| 8-bit immediate | `$42` |
| 16-bit immediate | `$0150` |
| absolute address | `($FF80)` |
| `(HL)` variants | `(HL)`, `(HL+)`, `(HL-)` |
| relative jump | print the **resolved target**: `JR $00F8`, not `JR -8` |
| conditionals | `NZ Z NC C` or `nz z nc c` — just be consistent |
| the 11 illegal opcodes | `illegal` (or `???` gives a hole that the coverage test would catch — decide deliberately) |

Resolving relative jumps is the one choice that pays off immediately: a trace full of
`JR $00F8` is scannable; a trace of `JR -8` requires arithmetic while you read.

### Using the tracer to find the first divergence

This is the workflow the rest of the course assumes:

```
.\gb\build\gbemu.exe --rom <game>.gb --frames 4 --trace build\t.txt
```

Then read the **tail** of the file. `gb_trace_print_last(gb, out, n)` and the fatal
handler's dump (`cpu_fatal` prints the last 32 entries) both exist for this. The technique:

1. find the last instruction whose *input* state was correct;
2. read the disassembly of the next few entries;
3. ask "what should the CPU have done here?" — compare against the instruction's definition;
4. that is your bug, and it is within three instructions of the end of the trace.

`m11_trace_ring_keeps_last_n` exists because this method depends on the ring buffer keeping
the **last** N instructions and dropping the oldest: a trace that kept the first 256 entries
would show you the boot ROM instead of the crash.

> **What matters for the code you are about to write**
>
> * Coverage is the contract: all **245 legal** base opcodes and all **256 CB** opcodes must
>   produce a mnemonic. The test asserts the output starts with a letter, which rejects both
>   the provided hex placeholder (`7E 00 00`) and a `???` hole. Everything else about
>   formatting is yours.
> * The six spot checks are `NOP`, `HALT`, `JP` (0xC3), and the CB mnemonics `RLC` (0xCB 0x00),
>   `BIT` (0xCB 0x7E), `SET` (0xCB 0xFF) — so use the conventional mnemonic names.
> * Operand bytes come from `gb->cart.rom`, bounded by `cart.rom_size`. Never `bus_read`:
>   it would tick the bus during tracing and would follow the current bank.
> * `gb_disasm` is called from **inside the tracer**, so it must be side-effect free: no
>   `bus_write`, no ticks, no allocation. Stack buffers only.
> * Use the regularity (`(op & 0xC7) == 0xC6`, the 8x8 `LD r,r'` matrix, `op >> 3` for the
>   CB groups) rather than 501 cases. It is less typing and it makes missing-cover easy to
>   see.
> * The illegal eleven print *something*; the coverage test skips them, so `illegal` is
>   fine. Do not print a hole for the other 245.
> * `m11_trace_ring_keeps_last_n` tests provided code (`debug.c`'s ring), so it is a
>   regression guard, not your work: if it fails, you have broken the tracer.

---

## 2. Your task — 55 min

Work in `gb/src/debug.c` (the body of `gb_disasm`), plus a helper or two if you want them.

1. **The register names and the base blocks** (25 min). Implement the six blocks from the
   table, then a jump table for the ~40 remaining one-off opcodes. Aim for "decode, do not
   enumerate".
2. **Operands** (15 min). Read operand bytes from `cart.rom` with bounds checks. Resolve
   relative targets. Get `(HL+)`/`(HL-)` right — `0x22`, `0x2A`, `0x32`, `0x3A` are their
   own opcodes, not `LD (HL),A` with a suffix.
3. **The CB page** (10 min). One table, one `(bit, reg)` split.
4. **Run the test** (5 min):
   ```
   .\gb\build.cmd -Test m11_disassembler
   ```
   The failure message names the exact opcode that did not decode, so work the list from
   the output.

---

## 3. Prove it — 10 min

```
.\gb\build.cmd -Test m11
```
```
   3 passed,    0 failed,    0 skipped
```
Then use it for the thing it exists for. Pick a moment you have already debugged — the L20
`cpu_instrs` bring-up, or a crash from your `TODO(hardware)` list — and read the trace:

```
.\gb\build\gbemu.exe --rom <a ROM with a known failure>.gb --frames 4 --trace build\t.txt
Get-Content build\t.txt -Tail 40
```

You are looking for the **first** instruction whose result you disagree with, not the last
one that printed. Write down the line number; the bug is within the next three instructions.
Do the same with `--break-op` on the opcode you suspect, which stops the machine and dumps
the registers and the trace right there.

Finally, confirm the tracer did not change behaviour, because `gb_disasm` runs inside it:

```
.\gb\build.cmd -Test
```
Same counts as before this lesson.

---

## 4. If it fails

| Symptom | Cause | Find it |
| --- | --- | --- |
| The test names the same opcode repeatedly | you are decoding an operand byte as an opcode | `LD A,$42` is 2 bytes; make sure you skip them |
| A whole block is missing | a `switch` range case is wrong | `0x40-0x7F` is `LD r,r'`; `0x76` inside it is `HALT` |
| `LD (HL)` prints as `LD (HL),A` for 0x22/0x2A | you special-cased instead of assigning `HL+`/`HL-` | give those four opcodes their own spelling |
| The trace has one line per *byte* | you advance by 1 instead of by the instruction length | return or track the length |
| The disassembler works but timing tests regressed | you called `bus_read` | read `cart.rom` |
| The ROM's last bytes crash | no bounds check | clamp operand reads |
| `m11_trace_ring_keeps_last_n` fails | you changed the ring while adding disassembly | restore `debug.c`'s ring logic |
| CB opcodes print the base mnemonic | you forgot to consume the second byte | `CB` is a prefix; the next byte is the opcode |

---

## 5. Done when

- [ ] `m11_disassembler_covers_all_ops` and `m11_trace_ring_keeps_last_n` are green
- [ ] `Get-Content build\t.txt -Tail 40` shows readable instructions for a real ROM
- [ ] `-Test` shows no regression, especially in the timing tests
- [ ] You found at least one bug by reading a trace rather than by guessing
- [ ] You can explain why the disassembler reads the ROM instead of the bus
- [ ] Commit message like `feat(debug): a full disassembler for the base and CB pages (L33)`

---

## 6. Optional, only if you have time

* Add `--disasm FILE:ADDR:LEN` to disassemble a region of a ROM, or a hotkey in L34's window
  that dumps the next 20 instructions from the current PC. Both are ten-minute jobs now that
  the decoder exists.
* `docs/06-verification-and-tooling.md` §6.5 — the fault-finding section that this lesson
  turns from a description into a tool.

Next: **[L34 — A real window](L34-a-real-window.md)**
