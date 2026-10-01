# Cheatsheet — flags, timing constants and the post-boot state

## Flag byte (F) — only the top nibble exists

| bit | 7 | 6 | 5 | 4 | 3..0 |
| --- | --- | --- | --- | --- | --- |
| name | Z | N | H | C | always read 0 |
| value | `0x80` | `0x40` | `0x20` | `0x10` | `0x00` |

Mask F with `0xF0` on every write, and on `POP AF`.

## Timing constants

| Name | Value |
| --- | --- |
| Master clock | 4194304 Hz (2^22) |
| 1 M-cycle | 4 T-cycles (one byte fetch) |
| Scanline | 456 T-cycles |
| Visible lines | 144 (LY 0-143) |
| VBlank lines | 10 (LY 144-153) |
| Frame | 154 x 456 = **70224 T-cycles** |
| Frame rate | 59.7275 Hz (16.74 ms) |
| DIV increment | every 256 T-cycles (16384 Hz) |
| Timer rates (TAC bits 1-0) | `00` bit 9 / 4096 Hz (1024 T) · `01` bit 3 / 262144 Hz (16 T) · `10` bit 5 / 65536 Hz (64 T) · `11` bit 7 / 16384 Hz (256 T) |
| Serial, internal clock | 8192 Hz: 512 T per bit, 4096 T per byte |
| OAM DMA | 160 M-cycles = 640 T-cycles (DMG) |

## Interrupts

| Bit | Source | Vector | Raised by |
| --- | --- | --- | --- |
| 0 | VBlank | `0040` | PPU entering mode 1 |
| 1 | LCD STAT | `0048` | rising edge of the enabled STAT source OR |
| 2 | Timer | `0050` | TIMA overflow |
| 3 | Serial | `0058` | transfer complete |
| 4 | Joypad | `0060` | high-to-low transition of a selected key bit |

Dispatch: `pending = IE & IF & 0x1F`; if `IME` and `pending`, take the **lowest**
set bit, clear `IME`, clear that IF bit, push PC, jump to the vector, 20 T-cycles.
`EI` takes effect after the next instruction; `RETI` sets `IME` immediately.

## Post-boot state (DMG, at `PC = 0x0100`)

Use this when you skip the boot ROM (`--no-boot-rom`, the default). Values are
from Pan Docs *Power Up Sequence*, cross-checked against mooneye's
`acceptance/boot_hwio-dmgABCmgb.s`. Pan Docs itself warns this table is
"highly volatile"; the two entries marked UNVERIFIED are genuinely
uninitialised on hardware.

| Register | Value | Notes |
| --- | --- | --- |
| AF | `01B0` | A=`01`; F=Z=1, N=0, H=1, C=1 (H/C depend on the header checksum) |
| BC | `0013` | |
| DE | `00D8` | |
| HL | `014D` | |
| SP | `FFFE` | |
| PC | `0100` | cartridge entry point |
| IME | 0 | interrupts are off when the game starts |
| IE | `0000` | |

| I/O | Value | | I/O | Value |
| --- | --- | --- | --- | --- |
| `FF00` P1 | `CF` | | `FF40` LCDC | `91` |
| `FF01` SB | `00` | | `FF41` STAT | `85` |
| `FF02` SC | `7E` | | `FF42` SCY | `00` |
| `FF04` DIV | `AB` | | `FF43` SCX | `00` |
| `FF05` TIMA | `00` | | `FF44` LY | `00` |
| `FF06` TMA | `00` | | `FF45` LYC | `00` |
| `FF07` TAC | `F8` | | `FF47` BGP | `FC` |
| `FF0F` IF | `E1` | | `FF48` OBP0 | UNVERIFIED (uninitialised) |
| `FF4A` WY | `00` | | `FF49` OBP1 | UNVERIFIED (uninitialised) |
| `FF4B` WX | `00` | | | |

Three facts that table cannot express, all from the same source:

* **`F` depends on the header checksum.** On DMG, `F` is Z=1, N=0, and **H and C are
  both set unless the cartridge header checksum at `0x014D` is `0x00`**, in which case
  both are clear (`F = 0x80`, `AF = 0x0180`). The `01B0` above assumes a non-zero
  checksum, which every real cartridge and this course's fixtures have.
* **`DIV = 0xAB` is the DMG/MGB column.** `DMG0` leaves `DIV = 0x18` and `LY = 0x91`.
  If a note you find says `DIV = 0x18`, it is quoting the `DMG0` column, not contradicting
  this one.
* **`OBP0`/`OBP1` are genuinely uninitialised** ("most often `0x00` or `0xFF`", and
  unreliable after a flashcart menu). Set them before the first sprite. Likewise WRAM
  and HRAM are random on power-up; a zeroed buffer is a teaching simplification.

The sound registers are also left in a defined state, and `dmg_sound` sub-test 01
reads them back before the game writes anything:

| Reg | `FF10` | `FF11` | `FF12` | `FF13` | `FF14` | `FF16` | `FF17` | `FF18` | `FF19` | `FF1A` | `FF1B` |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Value | `80` | `BF` | `F3` | `FF` | `BF` | `3F` | `00` | `FF` | `BF` | `7F` | `FF` |

| Reg | `FF1C` | `FF1D` | `FF1E` | `FF20` | `FF21` | `FF22` | `FF23` | `FF24` | `FF25` | `FF26` | `FF46` |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Value | `9F` | `FF` | `BF` | `FF` | `00` | `00` | `BF` | `77` | `F3` | `F1` | `FF` |

(`NR52` is `F1` on DMG/MGB and `F0` on SGB/SGB2. `FF46` reads `FF` on DMG and `00`
on CGB.) The oracle for all of this is mooneye
`acceptance/boot_hwio-dmgABCmgb`; Pan Docs itself warns that its post-boot table is
"highly volatile".

`m06_boot_without_bootrom` asserts only AF, BC, DE, HL, SP, PC, IME, LCDC and IE.
It deliberately does not assert `DIV` or `IF`.

## CPU quick facts

* Every instruction is 1, 2 or 3 bytes; the first byte is the opcode.
* `PC` points at the next byte to fetch; operand fetches advance it.
* All 16-bit immediates and addresses are **little endian**.
* Illegal opcodes (11): `D3 DB DD E3 E4 EB EC ED F4 FC FD`.
* `HALT` stops until `IE & IF != 0`. With `IME == 0` and a pending interrupt it
  does not halt, and the next byte is fetched twice (the halt bug).
* `(HL)` is operand index 6 and costs 4 extra T-cycles in every block that uses
  the r8 operand space.

## `CB`-prefix cycle conventions

`gbctr` and the gb-opcodes tables quote these differently, so state your
convention in your own notes. This course (and the tests) uses the **inclusive**
totals, which already contain the 4 T-cycles of the prefix fetch:

| Instruction | Inclusive | Prefix fetch | Sub-opcode only |
| --- | --- | --- | --- |
| `CB` rotate/shift or `RES`/`SET` on a register | 8 T | 4 T | 4 T |
| `CB` rotate/shift or `RES`/`SET` on `(HL)` | 16 T | 4 T | 12 T |
| `BIT r` | 8 T | 4 T | 4 T |
| `BIT (HL)` | 12 T | 4 T | 8 T |

`op_execute()` returns the inclusive total (it is called after the `0xCB` byte
has already been fetched by the dispatcher), so do not add 4 again.

## Where to verify these numbers

| Fact | Source |
| --- | --- |
| Post-boot state | Pan Docs `Power_Up_Sequence`, mooneye `boot_hwio-dmgABCmgb` |
| Opcode cycles | `reference/tables/gbctr-opcodes.toml`, `reference/tables/gb-opcodes-Opcodes.json` |
| Illegal opcodes | Pan Docs `CPU_Instruction_Set`, gbctr `opcodes.toml` |
| Timer quirks | Pan Docs `Timer_Obscure_Behaviour` |
| VRAM/OAM access | Pan Docs `Accessing_VRAM_and_OAM` |
| OAM priority | Pan Docs `OAM` (`rendering`), dmg-acid2 |

Links with annotations: `reference/external-references.md`.
