# 03 — The bus, the memory map, and the cartridge

## 3.1 One bus, two functions

Everything the CPU can touch goes through the bus:

```c
u8   bus_read (gb_t *gb, u16 addr);
void bus_write(gb_t *gb, u16 addr, u8 v);
u16  bus_read16 (gb_t *gb, u16 addr);   /* two 8-bit reads, little endian */
void bus_write16(gb_t *gb, u16 addr, u16 v);
void bus_tick (gb_t *gb, u32 tcycles);  /* advance timer, PPU, APU, serial */
```

Decode with a range chain. Do not allocate a flat 64 KiB array — you would hide
banking, and banking is the whole point of the cartridge.

## 3.2 The memory map

| Range | Size | Contents |
| --- | --- | --- |
| `0000-3FFF` | 16 KiB | ROM bank 0, **or the boot ROM while `FF50.0 == 0`** |
| `4000-7FFF` | 16 KiB | switchable ROM bank (from the MBC) |
| `8000-9FFF` | 8 KiB | VRAM (CGB: bank 0/1 via `FF4F`) |
| `A000-BFFF` | 8 KiB | cartridge RAM, banked by the MBC |
| `C000-CFFF` | 4 KiB | WRAM bank 0 (fixed) |
| `D000-DFFF` | 4 KiB | WRAM bank 1 (CGB: switchable) |
| `E000-FDFF` | 7.5 KiB | **echo** of `C000-DDFF` |
| `FE00-FE9F` | 160 B | OAM (40 sprites x 4 bytes) |
| `FEA0-FEFF` | 96 B | prohibited; reads `0x00`/`0xFF`, writes ignored |
| `FF00-FF7F` | 128 B | I/O registers |
| `FF80-FFFE` | 127 B | HRAM (fast internal RAM) |
| `FFFF` | 1 B | `IE` interrupt enable |

Rules that ROMs actually depend on:

* **Unmapped reads return `0xFF`.** Not `0x00`. Some ROMs probe the map to guess
  the hardware revision.
* **Echo RAM is a real mirror**: `bus_read(a)` for `E000-FDFF` must behave exactly
  like `C000-DDFF`, and writes must land in WRAM at `addr - 0x2000`.
* **I/O reads have side effects.** Never cache a value read from `FF00-FF7F`;
  `LY`, `DIV` and `TIMA` change under you, and reading `IF` is not a plain array read.
* **16-bit access crossing a bank boundary** must be two 8-bit accesses. Never
  cast an address to a pointer into a flat array.
* VRAM and OAM are inaccessible while the PPU is drawing (mode 3); reads return
  `0xFF`. Implement this in M09 — but know it exists.

## 3.3 The boot ROM

256 bytes mapped at `0000-00FF` while `FF50` bit 0 is 0. It:

1. verifies the 48-byte Nintendo logo at `0104-0133` against its internal copy
   (the copy protection "lockout"),
2. writes the initial values of the sound registers,
3. clears VRAM, sets up the palettes and `LCDC = 0x91`,
4. leaves a documented register state,
5. writes `FF50` to unmap itself, so the next fetch (`PC = 0x0100`) comes from
   the cartridge.

You have two options:

| Option | How | When |
| --- | --- | --- |
| **Skip** | set the post-boot register state from `reference/cheatsheet-flags-and-timing.md`, `PC = 0x0100`, `SP = 0xFFFE` | do this first; `--no-boot-rom` is the default |
| Emulate | load a boot ROM dump you made yourself from your own hardware | M12 / accuracy work only — it is copyrighted and cannot be shipped |

Skipping is not cheating; almost every emulator ships that way. It costs you
exactly one thing: if your post-boot state is wrong, a game may misbehave in a
way that looks like an unrelated bug. So when a game fails *before* executing any
of its own code, revisit the post-boot table first.

The unmapping trick is worth internalising: the boot ROM's final instruction
sits at `0x00FE`; it writes `FF50`, which removes the boot ROM from the bus
*while the instruction is still executing*. The following fetch at `0x0100` is
served by the cartridge. That means `bus_read` must consult the boot-ROM-enabled
flag on **every** access, not only at reset.

## 3.4 The cartridge header

| Offset | Field |
| --- | --- |
| `0100-0103` | entry point: `NOP; JP 0150` |
| `0104-0133` | Nintendo logo (48 bytes, checked by the boot ROM) |
| `0134-0143` | title (11-15 bytes, ASCII, zero-padded) |
| `013F-0142` | manufacturer code (newer carts) |
| `0143` | CGB flag: `0x80` CGB+DMG, `0xC0` CGB only |
| `0144-0145` | new licensee code |
| `0146` | SGB flag |
| `0147` | **cartridge type / MBC** |
| `0148` | **ROM size** |
| `0149` | **RAM size** |
| `014A` | destination code (0 = Japan, 1 = overseas) |
| `014B` | old licensee code |
| `014C` | mask ROM version |
| `014D` | header checksum |
| `014E-014F` | global checksum (often not filled in correctly) |

**Header checksum** (`014D`):

```
x = 0
for addr = 0x0134 .. 0x014C:
    x = x - rom[addr] - 1        /* all arithmetic mod 256 */
/* the stored byte must equal x */
```

**ROM size** `0148`: `0x00 -> 32 KiB`, then `0x01 -> 64`, `0x02 -> 128`,
`0x03 -> 256`, `0x04 -> 512`, `0x05 -> 1 MiB`, `0x06 -> 2 MiB`, `0x07 -> 4 MiB`,
`0x08 -> 8 MiB`. A few odd values (`0x52`, `0x53`, `0x54`) exist for legacy carts.

**RAM size** `0149`: `0x00` none, `0x01` 2 KiB (unused in practice), `0x02` 8 KiB,
`0x03` 32 KiB (four banks), `0x04` 128 KiB, `0x05` 64 KiB.

Validation policy: **warn, never reject.** Report the header checksum result, the
logo check, and any mismatch between the declared ROM size and the actual file
size in `--info`. Homebrew and hacks break every one of these rules. Never enforce
the logo check yourself — that is the boot ROM's job, and it is the only place the
check belongs.

## 3.5 MBCs: the conceptual unlock

A cartridge ROM is read-only, so "writing to the ROM area" cannot store anything.
What actually happens: the MBC **snoops the write**. The address range is a
*command*, and the byte written is the *argument*. A write to `0x2000-0x3FFF`
does not put data in ROM — it loads a bank number into a latch, and the latch
changes which 16 KiB of ROM appears at `0x4000-0x7FFF` on the next read.

Once you see that, MBC code becomes ten lines per register.

| Cart type `0147` | MBC |
| --- | --- |
| `0x00` | none (ROM only, 32 KiB or less) |
| `0x01-0x03` | MBC1 (+RAM, +battery) |
| `0x05-0x06` | MBC2 (+battery) |
| `0x08-0x09` | ROM + RAM (+battery), no MBC |
| `0x0B-0x0D` | MMM01 |
| `0x0F-0x13` | MBC3 (+RAM, +battery, +RTC) |
| `0x19-0x1E` | MBC5 (+RAM, +battery, +rumble) |
| `0x20` | MBC6 |
| `0x22` | MBC7 + sensor + rumble |
| `0xFC` | Pocket Camera |
| `0xFE` | HuC3 |
| `0xFF` | HuC1 |

### MBC1

| Window | Meaning |
| --- | --- |
| `0000-1FFF` | RAM enable: low nibble `0x0A` enables cartridge RAM |
| `2000-3FFF` | ROM bank, low 5 bits. **Writing 0 selects bank 1** |
| `4000-5FFF` | secondary bank (2 bits): upper ROM bits, or RAM bank in mode 1 |
| `6000-7FFF` | mode: 0 = ROM banking, 1 = RAM banking |

* The "bank 0 -> bank 1" substitution is a required quirk. A game relying on it
  will crash on the vector table if you skip it.
* Mode 1 on a 512 KiB cable-wired cart ("large ROM" wiring) means the 5-bit
  register becomes 4-bit + the 2-bit register becomes the top bits. Get the
  32 KiB-cart case working first; then read the MBC1 section of Pan Docs slowly.
* Bank masking: always `bank &= rom_bank_count - 1` *after* the substitution.

### MBC2

* `0000-3FFF` doubles as RAM enable and ROM bank select: if address bit 8 is 0 it
  is the enable, if 1 it is the bank.
* 512 x 4 bits of internal RAM at `A000-A1FF` (the upper nibble reads as `0xF0`).
* No external RAM; battery-backed.

### MBC3

| Window | Meaning |
| --- | --- |
| `0000-1FFF` | RAM/RTC enable (`0x0A`) |
| `2000-3FFF` | ROM bank, 7 bits |
| `4000-5FFF` | RAM bank 0-3, or RTC register `0x08-0x0C` |
| `6000-7FFF` | latch clock data: write `0x00` then `0x01` |

RTC registers: `08` seconds, `09` minutes, `0A` hours, `0B` days low, `0C` days
high + halt/overflow flags. Read the "day counter carry" bit field carefully; the
RTC is optional for your first commercial game.

### MBC5

| Window | Meaning |
| --- | --- |
| `0000-1FFF` | RAM enable (`0x0A`) |
| `2000-2FFF` | ROM bank, low 8 bits — **0 is legal** |
| `3000-3FFF` | ROM bank, 9th bit (`0` or `1`) |
| `4000-5FFF` | RAM bank 0-15 (most carts wire 0-3) |

MBC5 is the cleanest and the most common cart in the wild for later games.

### Save RAM

Allocate `ram_size` from the header, and persist it as `<rompath>.sav`:

* load at `cart_load()` if the file exists,
* mark the RAM dirty on every write,
* save on exit and on request.

Keep the .sav format raw (just the bytes) so it interoperates with other emulators.

## 3.6 OAM DMA

Writing to `FF46` starts a 160-byte copy: source = `value << 8`, destination =
`FE00`. On real hardware it costs 160 M-cycles and the CPU can only touch HRAM
during it. Implementation order:

1. M01-M06: copy instantly. Nothing observable breaks for most games.
2. M09: add the 160-cycle stall and the HRAM-only restriction (needed by
   `oam_bug` and a handful of games).

## 3.7 Bus traps

* Echo RAM writes silently going nowhere (you forgot the `- 0x2000`).
* Unmapped reads returning `0x00`.
* Treating `FF00-FF7F` as a plain array: `STAT` low bits, `TIMA`, `DIV`, `LY`
  and `IF` all need special read/write behaviour.
* Peripheral code that changes state but forgets to raise its `IF` bit.
* `bus_read16` implemented as a single decode (breaks at bank edges and on I/O).
* ROM bank count derived from the file size *or* the header, inconsistently used
  in the two places that need it (`cart_read` and the MBC register).
* Allocating RAM from the header but reading `A000-BFFF` through the ROM table.

## Sources

* **Pan Docs:** *Memory Map* (the address map, echo RAM, the prohibited range),
  *The Cartridge Header* (offsets and the checksum algorithm), *Power Up Sequence*
  (what the boot ROM does and the post-hand-off state), *OAM DMA Transfer*,
  *MBC1* / *MBC3* / *MBC5* (the `Cartridges` section), *MBC2*.
* **gbctr** for the boot-ROM hand-off details and the DMG/`DMG0`/`MGB`/`SGB`/CGB
  differences that explain why a post-boot value may look "wrong".
* The verified post-boot table (including `DIV`, `LY` and the `DMG0` column) is in
  [../reference/cheatsheet-flags-and-timing.md](../reference/cheatsheet-flags-and-timing.md).
* Oracles: mooneye `acceptance/boot_hwio-dmgABCmgb` and `boot_regs-dmgABC` for the
  post-boot state, `bits/unused_hwio-C` for unmapped I/O reads. Annotations:
  [../reference/external-references.md](../reference/external-references.md).

Next: **[04-ppu-and-peripherals.md](04-ppu-and-peripherals.md)** — 70224 cycles,
and turning two bitplanes into a picture.
