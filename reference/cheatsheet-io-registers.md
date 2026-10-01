# Cheatsheet — I/O registers (`FF00-FF7F`)

`R` readable, `W` writable, `-` unused. "Owner" is the struct member that holds
the value: a register lives in exactly one place, and only `bus_read()` /
`bus_write()` translate addresses into it (see `gb/include/gb/bus.h`).

| Addr | Name | Owner | R/W | Bits |
| --- | --- | --- | --- | --- |
| `FF00` | P1/JOYP | `joypad.select` | R/W | 0-3 key state (0 = pressed), 4 P14 directions, 5 P15 buttons, 6-7 read 1 |
| `FF01` | SB | `serial.sb` | R/W | shift byte |
| `FF02` | SC | `serial.sc` | R/W | 7 start/done, 1 internal clock, 0 shift clock |
| `FF03` | — | `bus.io[3]` | - | unused |
| `FF04` | DIV | derived | R/W | upper 8 bits of `timer.div_counter`; **writing resets the counter** |
| `FF05` | TIMA | `timer.tima` | R/W | timer counter |
| `FF06` | TMA | `timer.tma` | R/W | reload value |
| `FF07` | TAC | `timer.tac` | R/W | 2 enable, 1-0 rate select, 3-7 read 1 |
| `FF08-FF0E` | — | `bus.io[]` | - | unused |
| `FF0F` | IF | `bus.io[0x0F]` | R/W | 0-4 interrupt requests, 5-7 read 1 |
| `FF10` | NR10 | `apu.regs[0x00]` | R/W | 6-4 sweep period, 3 direction, 2-0 shift |
| `FF11` | NR11 | `apu.regs[0x01]` | R/W | 7-6 duty, 5-0 length load |
| `FF12` | NR12 | `apu.regs[0x02]` | R/W | 7-4 volume, 3 direction, 2-0 period |
| `FF13` | NR13 | `apu.regs[0x03]` | W | 2-0 frequency low |
| `FF14` | NR14 | `apu.regs[0x04]` | R/W | 7 trigger, 6 length enable, 2-0 frequency high |
| `FF15` | — | | - | unused |
| `FF16` | NR21 | | R/W | duty / length (as NR11) |
| `FF17` | NR22 | | R/W | envelope (as NR12) |
| `FF18` | NR23 | | W | frequency low |
| `FF19` | NR24 | | R/W | trigger / length / frequency high |
| `FF1A` | NR30 | | R/W | 7 DAC enable (CH3) |
| `FF1B` | NR31 | | W | 7-0 length |
| `FF1C` | NR32 | | R/W | 6-5 output level |
| `FF1D` | NR33 | | W | frequency low |
| `FF1E` | NR34 | | R/W | trigger / length / frequency high |
| `FF1F` | — | | - | unused |
| `FF20` | NR41 | | W | 5-0 length |
| `FF21` | NR42 | | R/W | envelope |
| `FF22` | NR43 | | R/W | 7-4 clock shift, 3 width mode, 2-0 divisor |
| `FF23` | NR44 | | R/W | 7 trigger, 6 length enable |
| `FF24` | NR50 | | R/W | 7 VIN left, 6-4 left volume, 3 VIN right, 2-0 right volume |
| `FF25` | NR51 | | R/W | 7-4 CH4-CH1 to left, 3-0 CH4-CH1 to right |
| `FF26` | NR52 | | R/W | 7 APU on/off, 3-0 channel status (read-only), 6-4 read 1 |
| `FF27-FF2F` | — | | - | unused |
| `FF30-FF3F` | wave RAM | `apu.wave[]` | R/W | 32 4-bit samples |
| `FF40` | LCDC | `ppu.lcdc` | R/W | 7 LCD on, 6 window map, 5 window on, 4 tile data, 3 BG map, 2 OBJ size, 1 OBJ on, 0 BG+window on/priority |
| `FF41` | STAT | `ppu.stat` | R/W | 6 LYC int, 5 mode 2 int, 4 mode 1 int, 3 mode 0 int, 2 LYC=LY flag (ro), 1-0 mode (ro). **Only bits 3-6 are writable** |
| `FF42-FF43` | SCY, SCX | `ppu.scy`, `ppu.scx` | R/W | background scroll |
| `FF44` | LY | `ppu.ly` | R | current scanline, **writes ignored** |
| `FF45` | LYC | `ppu.lyc` | R/W | scanline to compare |
| `FF46` | DMA | `bus.dma_*` | W | writing `X` copies `X<<8`..`X<<8+0x9F` to `FE00` |
| `FF47` | BGP | `ppu.bgp` | R/W | BG palette, 2 bits per colour id |
| `FF48-FF49` | OBP0, OBP1 | | R/W | OBJ palettes |
| `FF4A-FF4B` | WY, WX | | R/W | window position (`WX - 7` is the screen x) |
| `FF4C-FF4F` | — | | - | unused on DMG (`FF4F` is CGB VRAM bank) |
| `FF50` | BOOT | `gb->boot_rom_enabled` | W | bit 0 = 0 keeps the boot ROM mapped; writing nonzero unmaps it |
| `FF51-FF7F` | — | `bus.io[]` | - | unused |

## Rules

* **Unused I/O reads are not uniformly `0xFF`.** Most DMG unused registers read
  `0xFF`; a few read `0x00`. Do not guess: `mooneye`'s `unused_hwio-C` test is the
  oracle, and it is short.
* Never cache a read from this range. `LY`, `DIV`, `TIMA` and `P1` change under
  you, and reading `STAT` updates bits 0-2 from the live PPU state.
* Writing `DIV` resets the internal counter — that is a side effect, not a store.
* `IF` bits are usually described as writable-and-clearable; the exact set/clear
  semantics on DMG is one of the things `mooneye` tests. Start with a plain write
  of bits 0-4, bits 5-7 reading 1, and revisit if `if_ie_registers` fails.
* Peripherals raise interrupts by *setting* the matching `IF` bit; they never
  touch `IE`. `bus_request_interrupt()` in `src/bus.c` is provided for that.
* `FF50` in this course is not stored in `io[]`: the boot ROM is mapped or not
  based on `gb->boot_rom_enabled`, which `bus_read()` must consult on **every**
  access (a write to `FF50` unmaps it mid-instruction).

## Ownership invariant

```
gb_t              all mutable machine state
bus_read/write    the only CPU path to any component
bus_tick          the only place the clock advances
component .c      behaviour only, never data
```
