# Glossary

Alphabetical. Bold terms are the ones the lessons use without re-explaining. Where a
term names a register, its address is included.

Conventions used throughout the course:

* One **T-cycle** is one tick of the 4.194304 MHz master clock; one **M-cycle** is four
  T-cycles, the time to fetch one byte.
* One video frame is 70224 T-cycles (about 59.7 Hz); one 48 kHz output sample is about
  87.4 T-cycles.
* Addresses are written as `0xFF40` or `FF40` in prose, and ranges as `0xC000-0xDFFF`.
* "DMG" qualifies behaviour of the original machine; "CGB" qualifies the Color Game Boy.
  A term with neither is common to both.
* "Reads as" always means what the CPU sees through the bus, which is not necessarily
  what you last wrote: many I/O registers are write-only.

| Term | Meaning |
| --- | --- |
| **acid2** | A PPU conformance ROM (dmg-acid2, cgb-acid2) that renders a picture designed to expose every rendering bug at once. Passing it is the usual "the picture is right" gate. |
| **APU** | Audio Processing Unit. The four-channel synthesiser at FF10-FF2F, clocked by T-cycles and drained by the host as 48 kHz stereo samples. |
| **bank** | One switchable window of a larger memory. A ROM bank is 16 KiB (`0x4000-0x7FFF`), a RAM bank is 8 KiB (`0xA000-0xBFFF`), and an MBC register chooses which bank appears there. |
| **bank masking** | Reducing a requested bank number with AND to the number the cartridge actually has, so an over-large bank number wraps instead of reading garbage. |
| **BG priority** | The rule deciding whether a sprite pixel hides a background pixel. Depends on the sprite's attribute bit and, on CGB, on the tile's attribute byte. |
| **blargg** | Author of the standard Game Boy test ROM suite: `cpu_instrs`, `instr_timing`, `mem_timing`, `oam_bug`, `dmg_sound`, `halt_bug`. Prints `Passed`/`Failed` over the serial port. |
| **boot ROM** | The 256-byte internal ROM mapped at `0x0000` at power-on that initialises the hardware, scrolls the logo, and unmaps itself. Emulators usually skip it and load a post-boot state. |
| **breakpoint** | A debugger condition that stops execution when it is met (usually a PC value or a memory access), so you can inspect state at the exact instruction that misbehaved. |
| **CGB** | Color Game Boy. A second hardware generation with double speed, 8 KiB VRAM, palette RAM and per-tile attributes. Selected by the header's CGB flag byte. |
| **CGB attribute byte** | On CGB, the byte in VRAM bank 1 at the same offset as a BG map tile number. Bits 0-2 palette, 3 tile VRAM bank (OBJ), 5 X-flip, 6 Y-flip, 7 BG-to-OAM priority. |
| **checksum** | The two header bytes at `0x014D-0x014E` that validate the cartridge header; a wrong checksum is how a ROM tells you it was modified or truncated. |
| **DAC** | The 4-bit digital-to-analog converter at the end of each APU channel. When it is off (NRx2 bits 7-3 all zero, or NR30 bit 7 clear) the channel is disconnected and silent. |
| **disassembler** | A tool that turns the bytes at a PC into readable instructions; its output is what makes a trace file useful. |
| **DIV** | The divider register at `FF04`, incremented every 256 T-cycles; writes reset it. Its upper bits are visible, so it doubles as a free-running clock that games read for timing. |
| **DMG** | Dot Matrix Game, the original 1989 Game Boy (`DMG-01`). "DMG behaviour" means the original hardware's quirks, as opposed to CGB behaviour. |
| **DMA** | Direct Memory Access: hardware copying memory without CPU instructions. On CGB, HDMA (`FF51-FF55`) does it once per HBlank or all at once. |
| **duty cycle** | The fraction of a square wave's period that is high. CH1/CH2 select 12.5%, 25%, 50% or 75% via an 8-step pattern in NRx1 bits 7-6. |
| **echo RAM** | `0xE000-0xFDFF`, a mirror of `0xC000-0xDDFF`. Writes through either address change the same bytes. |
| **EI delay** | The one-instruction delay after `EI` before interrupts are actually enabled, so the instruction following `EI` always runs even if an interrupt is pending. |
| **envelope** | The APU unit that ramps a square or noise channel's volume up or down over time, stepping at 64 Hz while enabled through NRx2. |
| **frame sequencer** | The APU's 512 Hz, 8-step global timer. It clocks length counters at 256 Hz, envelopes at 64 Hz and sweep at 128 Hz. |
| **halt** | The instruction that stops the CPU until an interrupt is pending. Different from `STOP`, which also gates the clock on CGB. |
| **halt bug** | A DMG bug where `HALT` with `IME = 0` and a pending interrupt makes the following byte execute twice, because the PC fails to increment. |
| **HBlank** | The horizontal blanking period between scanlines, PPU mode 0. Short (about 204 T-cycles) and the standard place to update VRAM. |
| **HDMA** | CGB-only DMA (`FF51-FF55`) that copies a block from ROM to VRAM either once per HBlank or all at once, freeing the CPU. |
| **HRAM** | High RAM, `0xFF80-0xFFFE`: 127 bytes of fast internal RAM, and the only memory a transfer routine can safely live in during OAM DMA. |
| **IF / IE / IME** | Interrupt Requested flags at `FF0F`, Interrupt Enable mask at `FFFF`, and the master Interrupt Master Enable flip-flop set by `EI` and cleared by `DI`. An interrupt fires only when all three agree. |
| **joypad** | The input register at `FF00`, read through the P1 bits: bit 4 (P14) selects the direction keys, bit 5 (P15) selects the buttons. |
| **LCDC** | The LCD Control register at `FF40`: LCD enable, window enable, tile-data select, BG-map select, sprite size, and which layers are visible. |
| **LFSR** | Linear-feedback shift register: the noise channel's 15-bit pseudo-random generator, which XORs bits 0 and 1 each clock. A width bit makes it 7-bit for a tonal buzz. |
| **LR35902** | The CPU's actual part name: an 8080/Z80 hybrid with the Z80's CB-prefix bit instructions but no IX/IY or shadow registers. |
| **LY / LYC** | The current scanline at `FF44` and the value it is compared against at `FF45`; a match raises the STAT LYC interrupt. |
| **MBC** | Memory Bank Controller: the chip inside the cartridge that maps ROM and RAM banks through registers written into the ROM address range. |
| **M-cycle** | Machine cycle: 4 T-cycles, the time to fetch one byte. Opcode timing tables are usually written in M-cycles. |
| **mode 0-3** | The PPU's four per-scanline states: 0 HBlank, 1 VBlank, 2 OAM search, 3 pixel transfer. Read from STAT bits 1-0. |
| **mooneye** | A modern test-ROM suite (mooneye-gb) covering CPU, timer and PPU timing with fine-grained pass/fail ROMs, complementary to blargg's. |
| **Nintendo logo** | The 48 bytes at `0x0104-0x0133` that the boot ROM checks; if they do not match the boot ROM refuses to continue. |
| **OAM** | Object Attribute Memory, `0xFE00-0xFE9F`: 40 sprites of 4 bytes (Y, X, tile, attributes), followed by 8 unused bytes. |
| **OAM DMA** | The `FF46` write that copies 160 bytes into OAM from a page of your choice in roughly 160 M-cycles, during which only HRAM is accessible. |
| **palette** | The lookup that maps a 2-bit pixel value to a shade. DMG has three fixed registers (BGP, OBP0, OBP1); CGB has 8 BG and 8 OBJ palettes in palette RAM. |
| **PPU** | Pixel Processing Unit: the scanline state machine that reads tiles, sprites and palettes and emits one 160x144 frame every 70224 T-cycles. |
| **ROM header** | The bytes at `0x0100-0x014F`: entry point, logo, title, CGB flag, cartridge type, ROM/RAM sizes, destination and checksums. |
| **save state** | A serialised snapshot of the entire machine, used to bisect bugs in time. It is only useful if *every* piece of state is in it. |
| **serial** | The link-port registers: data at `FF01` (SB) and control at `FF02` (SC). Test ROMs use it as a text output port for `Passed`/`Failed`. |
| **sprite / OBJ** | A movable 8x8 or 8x16 graphic drawn from OAM, up to 10 per scanline, with attributes for palette, priority and flips. |
| **STOP** | The instruction that halts the CPU and the LCD until a button press. On CGB it also performs the speed switch when `KEY1` bit 0 was set. |
| **sweep** | The CH1-only unit that periodically retunes its own frequency by adding or subtracting a shifted copy, used for rising and falling pitch effects. |
| **T-cycle** | One tick of the 4.194304 MHz master clock, and the unit of all timing: a frame is 70224 T-cycles and one 48 kHz output sample is about 87.4. |
| **test ROM** | A ROM that exercises hardware behaviour and reports pass or fail, usually by printing over the serial port rather than showing a picture. |
| **tile** | An 8x8 pixel graphic, 16 bytes, stored as two bitplanes so each pixel is a 2-bit index. |
| **tile data** | The tile graphics pool, addressed either as signed numbers from `0x9000` or unsigned from `0x8000`, depending on LCDC bit 4. |
| **tile map** | The 32x32 grid of tile numbers at `0x9800` or `0x9C00` saying which tile goes in each background or window cell. |
| **TIMA** | The timer counter at `FF05`, incremented at the rate TAC selects; overflow reloads it from TMA and raises an interrupt. |
| **TMA** | The timer modulo at `FF06`: the value TIMA reloads to when it overflows. |
| **TAC** | The timer control at `FF07`: enable bit 2 and clock-select bits 0-1 (4096, 262144, 65536 or 16384 Hz). |
| **tracer** | The ring buffer in `cpu_step` recording the last N instructions; the first tool to reach for when state goes wrong. |
| **VBlank** | The region where the PPU draws nothing, scanlines 144-153. The safe place to update VRAM and OAM, and where the VBlank interrupt fires. |
| **VRAM** | Video RAM, `0x8000-0x9FFF`: tile data and both tile maps, plus on CGB the attribute bytes in a second bank. |
| **WAV** | The uncompressed audio container that `--wav` writes: a RIFF header plus 16-bit little-endian samples. It exists so the APU can be checked by ear and by FFT. |
| **wave RAM** | The 16 bytes at `FF30-FF3F` holding CH3's 32 4-bit samples, played back at a programmable rate. |
| **window** | A second background layer positioned by WX/WY and enabled by LCDC bit 5, used for status bars and menus. Its coordinates are offset by 7 from where you expect. |
| **WRAM** | Work RAM, `0xC000-0xDFFF`: 8 KiB on DMG, and 32 KiB banked on CGB. General-purpose memory for the game. |
| **X-flip / Y-flip** | Sprite attribute bits, and on CGB tile attribute bits, that mirror a tile horizontally or vertically while drawing it. |
| **zombie mode** | The DMG behaviour where an APU channel's volume register is transiently wrong after an NRx2 write while the channel is active, letting a stale volume reappear. |
