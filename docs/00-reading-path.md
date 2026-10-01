# 00 — Reading path: how to actually learn this

This document exists to answer one question: **what do I read, in what order, and
how do I know I learned it?**

The other docs are theory. The lessons are workflow. This one is the map between
them, and between this repo and the primary sources the whole field uses.

## 0.1 What is in this repository, and what is not

| You have, offline | Size | What it is |
| --- | --- | --- |
| `docs/01-06` | ~1,300 lines | Self-contained theory: orientation, CPU, bus/cartridge, PPU/peripherals, APU, verification |
| `lessons/M00-M12` | ~1,400 lines | The briefs: contracts, work order, traps, hint ladders |
| `reference/cheatsheet-*.md` | ~270 lines | Opcode map, flag rules, post-boot state, I/O register table |
| `reference/glossary.md` | ~80 lines | 64 terms, concrete, no circular definitions |
| `reference/external-references.md` | ~250 lines | Verified links with one-line annotations, grouped by purpose |
| `reference/tables/` | 2 files | Machine-readable opcode tables (CC0 and MIT) — the sources behind the cheatsheet |

| You do **not** have here | Why | Where to get it |
| --- | --- | --- |
| Pan Docs itself | It is big and changes; a snapshot would rot | `git clone https://github.com/gbdev/pandocs` (see 0.4) |
| gbctr (the 176-page reference PDF) | Same reason | <https://gekkio.fi/files/gb-docs/gbctr.pdf> |
| Test ROMs | Copyright and size | `roms/README.md`, and `reference/external-references.md` |
| The boot ROM | Copyrighted | Do not download it; the course skips it by design |
| A working emulator | That is the point | You |

The theory docs are written so you can complete **every milestone except M12**
using only what is in this repo. The primary sources are for when you want to
*understand deeper* or *disagree with me* — and you should do both.

## 0.2 Pick a track

| Track | For | How |
| --- | --- | --- |
| **Build-first** (recommended) | you learn by making something run | read a lesson, read only the theory section it names, implement, run tests. Theory arrives when it is needed, and every fact has a test attached to it within the hour. |
| **Theory-first** | you want the machine in your head before touching code | docs/01 -> 02 -> 03 -> 04 -> 05 -> 06 in order, then start M00. Slower to first green test, and you will forget half of it before you need it. |
| **Reference-driven** | you already know the hardware | skim docs/02 §2.4 and the three cheatsheets, then start at M00 and consult the docs only when a test fails. |

Whichever track: **do the milestones in order.** They are cumulative — M03's tests
cannot pass before M01's cartridge loader exists.

## 0.3 Topic -> course doc -> primary source -> oracle

This is the table to bookmark. When a milestone confuses you, read the course doc
first (it is short and sequenced for this project), then the primary source (it is
authoritative and long), and then let the oracle decide who is right.

| Topic | Course doc | Primary source (Pan Docs unless noted) | Oracle that settles it |
| --- | --- | --- | --- |
| What an emulator is, the step loop, tracing | `docs/01` | — (engineering, not hardware) | your own tracer output |
| Registers, flag byte, F's low nibble | `docs/02` §2.1 | *CPU Registers and Flags* | `m00_harness_selfcheck`, `m03_*` |
| Flag rules for every ALU family | `docs/02` §2.4 | *CPU Instruction Set* (flag columns) | `m03_alu_*`, then `cpu_instrs` |
| `DAA` | `docs/02` §2.4 | *CPU Instruction Set* | `m03_daa`, then `cpu_instrs` |
| Fetch/decode, `(HL)` operand, r8 space | `docs/02` §2.5 | *CPU Instruction Set* | `m02_*`, `m03_ld_r_r_matrix` |
| Interrupts, `EI` delay, `HALT` bug | `docs/02` §2.6 | *Interrupts*, *halt* | `m05_*`, `halt_bug.gb`, `interrupt_time.gb` |
| Opcode cycles and the `CB` page | `docs/02` §2.3, `reference/cheatsheet-opcode-map.md` | *CPU Instruction Set*; gbctr `opcodes.toml` | `instr_timing.gb`, `mem_timing.gb` |
| Memory map, echo RAM, unmapped reads | `docs/03` §3.2 | *Memory Map* | `m01_*`, mooneye `bits/unused_hwio-C` |
| Boot ROM and the post-boot state | `docs/03` §3.3 | *Power Up Sequence* | `m06_boot_without_bootrom`, mooneye `acceptance/boot_hwio-dmgABCmgb` |
| Cartridge header | `docs/03` §3.4 | *The Cartridge Header* | `m01_header_parse`, `--info` vs a hexdump |
| MBC1/2/3/5 banking | `docs/03` §3.5 | *MBC1*, *MBC3*, *MBC5* (Pan Docs `Cartridges` section) | `m08_*`, a real game that saves |
| OAM DMA | `docs/03` §3.6 | *OAM DMA Transfer* | `m09_oam_dma_timing`, `oam_bug.gb` |
| PPU modes, LY, the 456-dot line | `docs/04` §4.1 | *Rendering*, *STAT*, *LCDC* | `m07_stat_modes_timing`, `dmg-acid2` |
| Tiles, BG, window | `docs/04` §4.2 | *Rendering*, *Palettes* | `m07_bg_*`, `m07_window_position` |
| Sprites, priority, the 10-per-line limit | `docs/04` §4.2 | *OAM* | `m07_sprite_*`, `dmg-acid2` |
| VRAM/OAM access restrictions | `docs/04` §4.3 | *Accessing VRAM and OAM* | `oam_bug.gb` |
| Joypad matrix | `docs/04` §4.4 | *Joypad* (`FF00`) | `m06_joypad_matrix` |
| Serial, and why test ROMs print | `docs/04` §4.5 | *Serial Data Transfer (Link Cable)* | `m05_serial_emits_byte`, then any blargg ROM |
| APU channels, frame sequencer, mixing | `docs/05` | *Audio*, *Audio Registers* | `m10_*`, `dmg_sound` 01-06 |
| How to verify anything at all | `docs/06` | the test ROM repos themselves | the oracles in `docs/06` §6.3 |

## 0.4 How to read Pan Docs (the part nobody tells you)

1. **Clone it, do not bookmark it.**
   `git clone https://github.com/gbdev/pandocs` gives you the whole reference as
   Markdown in `src/`, offline, greppable, and diffable. This is a strictly better
   study tool than the website. gbctr's source is worth cloning too
   (`git clone https://github.com/Gekkio/gb-ctr`), because it carries per-instruction
   M-cycle timing diagrams the PDF alone makes hard to search.
2. **Start from the register, not the chapter.** Every question in this project
   arrives as "what does `FF41` do?". Pan Docs is organised that way: go to the
   register's page, read its table, then follow the links it gives you.
3. **Read the "Obscure Behaviour" pages last, but do read them.** They are the
   difference between an emulator that passes `dmg-acid2` and one that passes the
   whole mooneye suite.
4. **Trust the tests over the prose where they disagree.** Pan Docs carries its own
   warning that parts of the post-boot table are "highly volatile" and "may contain
   errors". That is why `reference/cheatsheet-flags-and-timing.md` marks two entries
   UNVERIFIED instead of inventing values, and why mooneye has `boot_hwio-dmgABCmgb`
   as the tie-breaker.
5. **Watch out for the DMG variants.** Several tables have a `DMG0` column that
   differs from DMG, MGB, SGB, CGB and AGB. If a number contradicts your notes,
   check which column you are in.

If you have no browser (or a scripted client): `gbdev.io` returns HTTP 403 to
non-browser clients on every path, but the same documents are served verbatim from
`https://raw.githubusercontent.com/gbdev/pandocs/master/src/<Page_Name>.md`
(verified: `Power_Up_Sequence.md` returns 200 and is where the post-boot table in
`reference/cheatsheet-flags-and-timing.md` comes from). `gekkio.fi` and
`raw.githubusercontent.com` answer normally.

## 0.5 Explain-back checks (how you know you learned it)

Tick these when you can answer out loud, in under a minute, with no notes. They
are ordered to match the milestones, and none of them requires code.

| After | You should be able to explain |
| --- | --- |
| docs/01 | why every memory access goes through one pair of functions, and what breaks if it does not |
| M01 | why writing to `0x2000-0x3FFF` changes what appears at `0x4000`, and why that is not a store into ROM |
| M01 | why reads from `FEA0-FEFF` must return `0xFF` rather than `0x00` |
| M02 | why `PC` needs no special handling when you add a new opcode, and what "the first byte is always the opcode" buys you |
| M03 | the exact rule for `H` and `C` on `ADD`, on `SUB`, and on `ADD SP,e8` — and why `AND` sets `H` |
| M03 | why `DAA` uses `0x60` in one place and `0x06` in the other |
| M04 | why `JR` is relative to the address *after* the instruction |
| M04 | why `POP AF` cannot restore the low nibble of `F` |
| M05 | why `EI` must not take effect immediately, in hardware terms |
| M05 | why `TIMA` is clocked by the *falling edge* of a bit of the divider, and what that implies for `DIV` writes |
| M06 | why a `HALT` loop without a PPU can never wake, and what that looks like in a trace |
| M07 | why sprites are not drawn in OAM order, and which two rules decide the winner |
| M07 | why the window must not use `LY` as its row |
| M08 | why bank masking is not optional, and where it belongs |
| M09 | why moving the clock into `bus_read` does not double-count time |
| M10 | why turning `NR52` bit 7 off silences everything, and why a channel's DAC being off is different |
| M12 | why the same ROM plus the same input gives the same trace, forever |

If you cannot answer one of these, you did not skip a step — you skipped a
paragraph. Go find it. That is the whole loop.

## 0.6 When you are blocked, read this

| Symptom | Read |
| --- | --- |
| "The opcode cycle counts feel arbitrary" | `docs/02` §2.3, then gbctr's instruction-set chapter |
| "My flags are subtly wrong" | `docs/02` §2.4 table, then `m03_*` — enumerate all 8 flag combinations |
| "It runs but nothing happens" | `docs/06` §6.7, then `docs/01` §1.7 and turn the tracer on |
| "A test ROM fails with a number" | `docs/06` §6.3, then the ROM's `readme.txt` in its repo |
| "The picture is almost right" | `docs/04` §4.7, then `dmg-acid2`'s reference image and its repo README |
| "Timing tests fail and everything else passes" | `docs/04` §4.1 + `docs/06` §6.3, then `instr_timing.gb`'s embedded cycle table |
| "I do not understand what MBC1 mode 1 does" | `docs/03` §3.5, then Pan Docs *MBC1* — and start with a 32 KiB MBC-less ROM |

## 0.7 What not to read

* **"Build a Game Boy emulator in an hour" blog posts.** They are not primary, they
  are frequently wrong about timing, and they will teach you a whole-instruction
  core you then have to unlearn in M09.
* **The Z80 or 8080 manuals.** The LR35902 is a hybrid; copy Z80 cycle counts or
  flag rules and you will pass your own tests while failing `cpu_instrs`. The only
  comparison worth reading is Pan Docs' *CPU Comparison with Z80*.
* **Other emulators' source code, before yours works.** As a *debugging* reference
  (SameBoy's `Core/`) it is excellent, and `docs/06` §6.1 lists it in the oracle
  hierarchy. As a first read it will hand you conclusions you cannot yet evaluate.
* **This course's hint ladders, before you have tried.** H3 is deliberately the last
  step before the answer. Reading it first converts the exercise into transcription.

## 0.8 Suggested schedules

| Pace | Plan |
| --- | --- |
| **Weekend sprint** (~20 h) | M00-M05 with `cpu_instrs` passing by Sunday night. Accept a black screen. This is the single most satisfying stopping point in the project. |
| **Two weeks** (~40 h) | M00-M07: a booting, correct-looking game plus `dmg-acid2`. |
| **Six weeks** (~100 h) | Everything through M11, with M09 spent on `mem_timing` rather than on new features. This is the pace that produces an emulator you would actually use. |

Whichever you pick, the schedule is not the point: the point is that every
milestone ends with a command whose output you can read, and no milestone can be
faked.

Next: **[01-orientation.md](01-orientation.md)** — or go straight to
**[../lessons/M00-setup-and-harness.md](../lessons/M00-setup-and-harness.md)** if
you would rather start building.
