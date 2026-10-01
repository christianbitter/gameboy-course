# Cheatsheet — opcode map

Reference data. Cycles are **T-cycles** (4 T = 1 M-cycle). Verify against Pan
Docs / gbctr if a timing test disagrees — but note that `mem_timing` is the real
oracle, not this table.

## Regular blocks (learn these rules, do not memorise 256 rows)

| Range | Meaning | T-cycles |
| --- | --- | --- |
| `0x40-0x7F` | `LD dst,src` with `dst = (op>>3)&7`, `src = op&7` | 4, or 8 if either side is `(HL)` |
| `0x76` | `HALT` (it is the `LD (HL),(HL)` slot) | 4 |
| `0x80-0xBF` | ALU `op A,reg` with `op = (op-0x80)>>3`, `reg = op&7` | 4, or 8 if `reg` is `(HL)` |
| `0xCB 0x00-0x3F` | rotate/shift `r8[op&7]` by `(op>>3)&7` | 8, or 16 for `(HL)` |
| `0xCB 0x40-0x7F` | `BIT n,r` with `n = (op>>3)&7`, `r = op&7` | 8, or 12 for `BIT (HL)` |
| `0xCB 0x80-0xBF` | `RES n,r` | 8, or 16 for `(HL)` |
| `0xCB 0xC0-0xFF` | `SET n,r` | 8, or 16 for `(HL)` |

ALU order for `0x80-0xBF`: `ADD, ADC, SUB, SBC, AND, XOR, OR, CP`.
Rotate order for `CB 0x00-0x3F`: `RLC, RRC, RL, RR, SLA, SRA, SWAP, SRL`.

**Decide once** whether the 4 T-cycles of the `0xCB` prefix fetch are included
in the counts above (they are, conventionally) or added by your dispatcher.

`r8` indices: `0=B 1=C 2=D 3=E 4=H 5=L 6=(HL) 7=A`.

## `0x00-0x3F`

| Op | Mne | T | Op | Mne | T | Op | Mne | T | Op | Mne | T |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 00 | NOP | 4 | 10 | STOP | 4 | 20 | JR NZ,e8 | 12/8 | 30 | JR NC,e8 | 12/8 |
| 01 | LD BC,d16 | 12 | 11 | LD DE,d16 | 12 | 21 | LD HL,d16 | 12 | 31 | LD SP,d16 | 12 |
| 02 | LD (BC),A | 8 | 12 | LD (DE),A | 8 | 22 | LD (HL+),A | 8 | 32 | LD (HL-),A | 8 |
| 03 | INC BC | 8 | 13 | INC DE | 8 | 23 | INC HL | 8 | 33 | INC SP | 8 |
| 04 | INC B | 4 | 14 | INC D | 4 | 24 | INC H | 4 | 34 | INC (HL) | 12 |
| 05 | DEC B | 4 | 15 | DEC D | 4 | 25 | DEC H | 4 | 35 | DEC (HL) | 12 |
| 06 | LD B,d8 | 8 | 16 | LD D,d8 | 8 | 26 | LD H,d8 | 8 | 36 | LD (HL),d8 | 12 |
| 07 | RLCA | 4 | 17 | RLA | 4 | 27 | DAA | 4 | 37 | SCF | 4 |
| 08 | LD (a16),SP | 20 | 18 | JR e8 | 12 | 28 | JR Z,e8 | 12/8 | 38 | JR C,e8 | 12/8 |
| 09 | ADD HL,BC | 8 | 19 | ADD HL,DE | 8 | 29 | ADD HL,HL | 8 | 39 | ADD HL,SP | 8 |
| 0A | LD A,(BC) | 8 | 1A | LD A,(DE) | 8 | 2A | LD A,(HL+) | 8 | 3A | LD A,(HL-) | 8 |
| 0B | DEC BC | 8 | 1B | DEC DE | 8 | 2B | DEC HL | 8 | 3B | DEC SP | 8 |
| 0C | INC C | 4 | 1C | INC E | 4 | 2C | INC L | 4 | 3C | INC A | 4 |
| 0D | DEC C | 4 | 1D | DEC E | 4 | 2D | DEC L | 4 | 3D | DEC A | 4 |
| 0E | LD C,d8 | 8 | 1E | LD E,d8 | 8 | 2E | LD L,d8 | 8 | 3E | LD A,d8 | 8 |
| 0F | RRCA | 4 | 1F | RRA | 4 | 2F | CPL | 4 | 3F | CCF | 4 |

## `0xC0-0xFF`

| Op | Mne | T | Op | Mne | T | Op | Mne | T | Op | Mne | T |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| C0 | RET NZ | 20/8 | D0 | RET NC | 20/8 | E0 | LDH (a8),A | 12 | F0 | LDH A,(a8) | 12 |
| C1 | POP BC | 12 | D1 | POP DE | 12 | E1 | POP HL | 12 | F1 | POP AF | 12 |
| C2 | JP NZ,a16 | 16/12 | D2 | JP NC,a16 | 16/12 | E2 | LD (C),A | 8 | F2 | LD A,(C) | 8 |
| C3 | JP a16 | 16 | D3 | **illegal** | - | E3 | **illegal** | - | F3 | DI | 4 |
| C4 | CALL NZ,a16 | 24/12 | D4 | CALL NC,a16 | 24/12 | E4 | **illegal** | - | F4 | **illegal** | - |
| C5 | PUSH BC | 16 | D5 | PUSH DE | 16 | E5 | PUSH HL | 16 | F5 | PUSH AF | 16 |
| C6 | ADD A,d8 | 8 | D6 | SUB d8 | 8 | E6 | AND d8 | 8 | F6 | OR d8 | 8 |
| C7 | RST 00H | 16 | D7 | RST 10H | 16 | E7 | RST 20H | 16 | F7 | RST 30H | 16 |
| C8 | RET Z | 20/8 | D8 | RET C | 20/8 | E8 | ADD SP,e8 | 16 | F8 | LD HL,SP+e8 | 12 |
| C9 | RET | 16 | D9 | RETI | 16 | E9 | JP HL | 4 | F9 | LD SP,HL | 8 |
| CA | JP Z,a16 | 16/12 | DA | JP C,a16 | 16/12 | EA | LD (a16),A | 16 | FA | LD A,(a16) | 16 |
| CB | prefix | - | DB | **illegal** | - | EB | **illegal** | - | FB | EI | 4 |
| CC | CALL Z,a16 | 24/12 | DC | CALL C,a16 | 24/12 | EC | **illegal** | - | FC | **illegal** | - |
| CD | CALL a16 | 24 | DD | **illegal** | - | ED | **illegal** | - | FD | **illegal** | - |
| CE | ADC A,d8 | 8 | DE | SBC A,d8 | 8 | EE | XOR d8 | 8 | FE | CP d8 | 8 |
| CF | RST 08H | 16 | DF | RST 18H | 16 | EF | RST 28H | 16 | FF | RST 38H | 16 |

`16/12` etc. means *taken / not taken*.

`LD (C),A` and `LD A,(C)` are `LD ($FF00+C),A` and `LD A,($FF00+C)`: the C
register supplies the low byte of the address, the high byte is `0xFF`.

## Illegal opcodes (11)

`0xD3 0xDB 0xDD 0xE3 0xE4 0xEB 0xEC 0xED 0xF4 0xFC 0xFD`

On real hardware these lock the CPU up (it stops fetching). In an emulator, the
useful behaviour is: if you ever execute one, abort loudly with a register dump.
A game *cannot* legitimately reach one, so hitting one means your decode or a
previous instruction already went wrong.

## Condition codes

| cc | Meaning | JR | JP | CALL | RET |
| --- | --- | --- | --- | --- | --- |
| NZ | `Z == 0` | 20 | C2 | C4 | C0 |
| Z | `Z == 1` | 28 | CA | CC | C8 |
| NC | `C == 0` | 30 | D2 | D4 | D0 |
| C | `C == 1` | 38 | DA | DC | D8 |

## `RST` vectors

| Op | Target | Op | Target |
| --- | --- | --- | --- |
| C7 | `0000` | E7 | `0020` |
| CF | `0008` | EF | `0028` |
| D7 | `0010` | F7 | `0030` |
| DF | `0018` | FF | `0038` |

## Flag effects at a glance

`-` = unchanged, `0`/`1` = forced, `*` = computed.

| Group | Z | N | H | C |
| --- | --- | --- | --- | --- |
| `ADD/ADC A,n` | * | 0 | * | * |
| `SUB/SBC n`, `CP` | * | 1 | * | * |
| `AND n` | * | 0 | **1** | 0 |
| `OR/XOR n` | * | 0 | 0 | 0 |
| `INC r` | * | 0 | * | - |
| `DEC r` | * | 1 | * | - |
| `ADD HL,rr` | - | 0 | * | * |
| `ADD SP,e8`, `LD HL,SP+e8` | 0 | 0 | * | * |
| `RLCA/RRCA/RLA/RRA` | 0 | 0 | 0 | * |
| `CB` rot/shift, `SWAP`, `SLA/SRA/SRL` | * | 0 | 0 | * (`SWAP`: 0) |
| `BIT n,r` | * | 0 | 1 | - |
| `CPL` | - | 1 | 1 | - |
| `SCF` | - | 0 | 0 | 1 |
| `CCF` | - | 0 | 0 | `!C` |
| `DAA` | * | - | 0 | * |
| loads, stack, jumps, `NOP` | - | - | - | - |

Post-boot register state, interrupt vectors and timing constants:
`reference/cheatsheet-flags-and-timing.md`.
