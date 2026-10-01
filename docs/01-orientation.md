# 01 — What an emulator is, and how we will build one

## 1.1 The mental model

A Game Boy is **a set of chips sharing one address bus and one clock**. There is no
magic anywhere. The CPU is a lookup table over 256 opcodes; the PPU is a small
scanline state machine; the timer is a counter with a comparator.

An emulator is therefore a **deterministic state machine with a clock**:

```
            +-------------------------------------------------+
            |                                                 |
   +--------+--------+   address/data   +---------+   +-------+--------+
   |  CPU (LR35902)  +------------------+  BUS    +---+  Timer (DIV/TIMA) |
   +-----------------+                  |         |   +--------------------+
                                        |         |   +--------------------+
        +--------------------+          |         +---+  PPU  (LY/STAT/LCD) |
        | Cartridge (ROM/RAM|<---------+         |   +--------------------+
        |   + MBC bank regs  |          |         |   +--------------------+
        +--------------------+          |         +---+  Joypad (FF00)     |
                                        |         |   +--------------------+
        +--------------------+          |         |   +--------------------+
        | WRAM / HRAM / VRAM <---------+         +---+  Serial (SB/SC)     |
        +--------------------+                      +--------------------+
```

The single most important consequence: **the CPU has no special powers**. It
cannot see VRAM, a cartridge bank, or the joypad. It can only read and write
addresses. So the *bus* is the spine of your emulator, and every component is
reachable through it.

Corollary: if you funnel *all* memory access through `bus_read()` / `bus_write()`,
you get these for free:

* a place to hang the tracer (every access is observable),
* a place to advance the clock (every access costs T-cycles),
* a single place to implement banking and I/O decoding.

## 1.2 The one function that runs the whole machine

Start with this loop and never abandon it:

```c
for (;;) {
    u32 cycles = cpu_step(&gb);   /* execute ONE instruction, return T-cycles used */
    bus_tick(&gb, cycles);        /* everything else advances by that many cycles */
    if (ppu_frame_ready(&gb)) { present_frame(&gb); read_input(&gb, &keys); }
}
```

`cpu_step()` is the "instruction-stepped core". Later you will refine it to
"access-stepped" (`bus_tick(4)` inside every memory access) for timing accuracy —
but only after the instruction-stepped version passes the CPU tests.

| Stepping granularity | Accuracy | Cost | When |
| --- | --- | --- | --- |
| Whole frame at a time | none | trivial | never (you lose all interrupts) |
| One instruction | ~95% of games | low | Milestones 02–06 |
| One memory access (4 T) | exact enough for every test ROM | medium | Milestone 09 |
| Sub-instruction / per-dot PPU | emulator-author bragging rights | high | never, unless you want to |

Design `bus_tick()` so you can *move* the call inside `bus_read`/`bus_write`
later without rewriting the CPU. That is the only architectural decision you
must get right up front.

## 1.3 What this is not

* Not a simulator: nothing is integrated over time; it is bookkeeping.
* Not a binary translator or JIT: you execute opcodes *one at a time*. If you ever
  find yourself "calling a function in the ROM", you have left emulation.
* Not parallel: everything is strictly sequential. That is a feature.
* Not a physics engine. Every number you will compute is an integer.

## 1.4 Determinism is your best debugging tool

Given the same ROM, the same initial state, and the same input sequence at the
same cycles, your emulator produces the *exact* same trace, forever. Exploit it:

1. **Bisect in time.** Keep save states. Notice the glitch at frame 900? Restore
   frame 899, step, watch the first wrong cycle.
2. **Diff two implementations.** Run your C core and your Python model on the
   same random opcode stream and report the *first* divergence. This finds bugs
   you would never think to test (Milestone 12).
3. **Replay bug reports.** A trace file plus a ROM is a complete reproduction.

The one rule: **never let host non-determinism leak into the machine.** No wall
clock, no RNG, no frame-rate-dependent logic inside the emulation core. Input is
fed in as "these buttons are held during cycles N..M".

## 1.5 Vertical slices, not horizontal layers

The classic beginner failure is to implement all 512 opcodes, then the bus, then
discover everything is wrong. Instead, make something *end-to-end runnable* at
each step:

| # | "It runs" milestone | What proves it |
| --- | --- | --- |
| 1 | PC advances and the tracer prints | `NOP` loop trace |
| 2 | A hand-written program runs | your CPU unit tests |
| 3 | A real ROM executes correctly | blargg `cpu_instrs` prints `Passed` |
| 4 | A real game boots | Tetris reaches its title screen |
| 5 | The picture is *right* | `dmg-acid2` passes |
| 6 | The picture is right *on time* | `instr_timing`, `mem_timing`, `oam_bug` |
| 7 | It makes sound | blargg `dmg_sound` sub-tests |

Notice that "boots a game" comes **before** "picture is right". A blank or
garbage screen plus working serial output is enough to boot most games.

## 1.6 Fail loudly, or lose a day

Every time you meet an opcode you have not implemented, print
`PC, opcode, register dump` and `abort()`. Never fall back to "treat as NOP".

A silently skipped instruction corrupts one register, which corrupts one jump,
which makes the game hang 4 minutes later, in a completely unrelated place. The
loud failure turns that into a five-second fix. Same for unimplemented I/O
registers: give them a name in your read/write switch and log the first access.

## 1.7 Build the tracer before you need it

In `cpu_step()`, record into a fixed ring buffer (last 256 entries):

| field | example |
| --- | --- |
| PC of the instruction | `0x01A7` |
| raw opcode bytes | `CD 9A 01` |
| decoded meaning | `CALL $019A` |
| registers after | `AF=01B0 BC=0013 DE=00D8 HL=014D SP=FFFE` |
| T-cycles consumed, total | `24`, `1048576` |

Add `--trace FILE` to dump it. This is the highest return-on-investment code in
the entire project. Every hard bug becomes: *find the last instruction that did
something impossible.*

## 1.8 Two loops you will live in

**The debug loop:** hypothesise → instrument → run → read trace → narrow.
If you are not narrowing, you are guessing; add instrumentation.

**The spec-reading loop:** when something is wrong, the answer is almost always a
sentence in Pan Docs that you skimmed. "The window is only rendered if WX is in
range." "`DAA` adds 0x60, not 0x06." "STAT mode 3's length varies." Rule of thumb
for an emulator: **90% of your bugs are in the 10% of the spec you read too fast.**

## 1.9 Vocabulary

| Term | Meaning |
| --- | --- |
| **T-cycle** | one tick of the 4.194304 MHz master clock; the unit of all timing |
| **M-cycle** | 4 T-cycles (= 1 byte fetched). Opcode tables are usually written in M-cycles |
| **DMG** | the original Game Boy (`DMG-01`); CGB is the Game Boy Color |
| **LR35902** | the CPU's actual part name; an 8080/Z80 hybrid |
| **Boot ROM** | 256-byte internal ROM at `0x0000` that initialises the machine |
| **MBC** | memory bank controller — a chip in the cartridge that maps ROM/RAM banks |
| **Bank** | a 16 KiB (ROM) or 8 KiB (RAM) window that "sliding" registers point at |
| **IF / IE / IME** | interrupt requested flags / enable mask / master enable |
| **VBlank** | the 10 scanlines after line 143 where the PPU does no drawing |
| **OAM** | the 40 hardware sprites, 4 bytes each, at `0xFE00` |
| **Tile** | 8×8 pixels, 16 bytes, 2 bits per pixel (two bitplanes) |
| **Palette** | maps the 2-bit pixel value 0–3 to one of 4 shades |
| **Blocking** | `PUSH`/`POP`/`CALL`/`RET` — anything touching the stack |
| **Save state** | a serialised snapshot of the whole machine; your time machine |
| **Test ROM** | a ROM that checks hardware behaviour and reports pass/fail |
| **Serial output** | the trick that lets test ROMs "print" text to your terminal |

## 1.10 How to use these artifacts

```
1. Read this doc, then docs/02..06 in order as your milestones need them.
2. Open lessons/Mnn-*.md for the milestone you are on.
3. Read its Goal + Deliverable contract + Acceptance tests.
4. Implement. Run the acceptance command. Only then tick TASKS.md.
5. Every milestone ends with a commit. Green tests, then commit.
```

You will write **all** of the emulator. The artifacts give you: the theory, the
API contract, the tests, the harness, the build, and the hint ladders — not the
implementations. If a lesson hands you code, it is infra (file loading, pixel
dumping, test macros), never emulation logic.

## Sources

This document is orientation, not hardware: everything specific it claims is sourced
in the document that owns the topic (see the matrix in
[00-reading-path.md](00-reading-path.md) §0.3). The design it teaches — one state
struct, one clock, all access through the bus — is an engineering choice, not a
hardware fact. Annotated links to every primary source used by the course:
[../reference/external-references.md](../reference/external-references.md).

Next: **[02-cpu.md](02-cpu.md)** — the CPU is where the work is.
