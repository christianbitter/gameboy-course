# M08 - Cartridges: MBCs, banking, and save RAM

**Goal** - Implement MBC1/MBC2/MBC3/MBC5 register behaviour on top of M01's header parsing, so ROM/RAM banks switch like the hardware and battery-backed RAM survives a process restart.

**Estimated effort** - 6-9 h (MBC1 ~1.5 h, MBC2 ~0.5 h, MBC3 and RTC ~2 h, MBC5 ~1 h, battery persistence ~1 h, real-game gating ~1.5 h).

## Read first

- `gb/include/gb/cart.h` - the frozen struct, the five functions and, importantly, the four **provided** helper functions. Read this file first: it decides what you write.
- `gb/src/cart.c` - the stub, the provided size tables, the provided `cart_print_info`, and the TODO block that states the validation policy.
- `docs/03-memory-and-cartridge.md`: 3.4 (header), 3.5 and the MBC1/MBC2/MBC3/MBC5 subsections plus Save RAM, 3.6 (OAM DMA, if you want context for 0xA000-0xBFFF), 3.7 (bus traps).
- `gb/tests/harness.h` and `gb/tests/t_m01_cart.c` for the fixture style, especially `t_build_rom`, which writes a minimal valid header for you.
- `docs/01-orientation.md` 1.7 (tracer) and the bank vocabulary in 1.9.
- Pan Docs: "The Cartridge Header" and the per-chip pages "MBC1", "MBC2", "MBC3", "MBC5".
- The `mooneye-gb` MBC test ROMs, one case each, which is exactly what you want for binary search.
- `docs/06-verification-and-tooling.md` for the serial harness that turns a test ROM's verdict into a readable line.

## Why this milestone exists

M01 already made a flat 32 KiB ROM run and left `cart.c` with working header fields, ROM reads and RAM sizing. A real cartridge is also a set of registers that remap a 16 KiB window at `0x4000` into a ROM many times larger, plus a second window of battery-backed RAM. Get the sizing wrong and a 1 MiB game executes the wrong bank and hangs; get MBC1's zero-bank quirk wrong and it hangs sooner; get RAM persistence wrong and the game saves and then forgets. All three look like CPU or PPU bugs from the outside, which is why this milestone is tested with synthetic fixtures rather than eyeballed games. The dispatch you write is also the shape of every hardware variant you will ever add.

## Deliverable contract

You implement the TODO block in `gb/src/cart.c`. The header is frozen; do not add, rename or reorder fields:

```c
bool cart_load (gb_t *gb, const u8 *data, size_t size, const char *path);
void cart_unload(gb_t *gb);                 /* provided */
u8   cart_read (gb_t *gb, u16 addr);
void cart_write(gb_t *gb, u16 addr, u8 value);
void cart_save (gb_t *gb);                  /* write <rom>.sav if dirty */
```

```c
/* frozen cart_t members - note bank_hi is MBC1's secondary 2-bit register */
u8  *rom;  size_t rom_size;      u8 *ram;  size_t ram_size;   /* both owned       */
mbc_type_t mbc;                  /* MBC_NONE, MBC_1, MBC_2, MBC_3, MBC_5, MBC_OTHER */
u8  cart_type, rom_size_code, ram_size_code;   bool battery, has_rtc;
bool ram_enabled;                u16 rom_bank;  u8 ram_bank, mode, bank_hi;
bool ram_dirty, header_checksum_ok, logo_ok;   char title[17];
```

Provided and not yours to write: `cart_mbc_name`, `cart_rom_size_bytes`, `cart_ram_size_bytes`, `cart_print_info`, `cart_unload`. Use them. `gbemu --info` is already correct once your `cart_load` fills the fields.

Validation policy, straight from the stub: **warn, never reject**. Only `cart_load` returning `false` for a file too small to hold a header or a declared ROM size that does not match the file. A bad logo or checksum is recorded in `logo_ok` / `header_checksum_ok` and reported, never fatal. `cart_load` copies the ROM into `gb->cart.rom`, sets `rom_size`/`ram_size` from the **header** tables (not the file size), allocates RAM, and loads `<rom>.sav` if it exists.

Address decode the bus calls into, 0x0000-0xFFFF:

| range | who answers |
| --- | --- |
| 0x0000-0x3FFF | ROM bank 0, or the remapped low bank (MBC1 mode 0) |
| 0x4000-0x7FFF | the switched ROM bank |
| 0xA000-0xBFFF | cart RAM, or an RTC register on MBC3 |
| 0x0000-0x1FFF | RAM enable |
| 0x2000-0x3FFF | ROM bank low bits |
| 0x4000-0x5FFF | RAM bank / MBC1 upper ROM bits |
| 0x6000-0x7FFF | MBC1 mode select, or the MBC3 RTC latch |

Type dispatch on `cart_type` (0x0147). `cart_mbc_name()` already names all of these; your job is the `mbc` enum:

| type | chip | notes |
| --- | --- | --- |
| 0x00 | none | 32 KiB ROM, no RAM, writes to 0x0000-0x7FFF ignored |
| 0x01-0x03 | MBC1 | +RAM, +battery |
| 0x05-0x06 | MBC2 | built-in 512 x 4-bit RAM at 0xA000-0xA1FF, +battery |
| 0x08/0x09 | none | ROM + RAM (+battery), still no bank register |
| 0x0B-0x0D | MMM01 | rare and unusual; map to MBC_OTHER and say so loudly |
| 0x0F-0x13 | MBC3 | 0x0F +timer+battery, 0x10 +timer+RAM+battery, 0x11 plain, 0x12 +RAM, 0x13 +RAM+battery |
| 0x19-0x1E | MBC5 | 0x1B +RAM+battery, 0x1C-0x1E add rumble |
| anything else | MBC_OTHER | warn once with the type byte, then behave as ROM-only |

Banking arithmetic, expressed once. `rom_bank_count = cart_rom_size_bytes(rom_size_code) / 0x4000`:

```
effective bank = reg & (rom_bank_count - 1)
MBC1: if ((reg & 0x1F) == 0) reg = 1, then mask
MBC1 mode 0: 0x4000-0x7FFF bank = low5 | (bank_hi << 5); 0x0000-0x3FFF = bank_hi << 5
MBC1 mode 1: 0x4000-0x7FFF bank = low5 | (bank_hi << 5); 0x0000-0x3FFF = 0; RAM bank = bank_hi
           /* mode 0 is what makes 1 MiB and 2 MiB MBC1 carts addressable: bank_hi
              selects the 0x00/0x20/0x40/0x60 window, the 0x200000/0x400000 sizes */
MBC5: rom_bank = low8_at_0x2000 | ((reg_0x3000 & 1) << 8)   /* 0..511, bank 0 selectable */
ROM offset = bank * 0x4000 + (addr & 0x3FFF)
```

Battery: `cart_save` writes exactly `ram_size` bytes to `gb->save_path` when `ram_dirty` is set and `battery` is true. `main.c` already calls it on exit, so your job is dirty tracking plus the write.

RTC gap to know about before you write the MBC3 test: the frozen `cart_t` has `has_rtc` but **no** fields for the five counters, their latched shadow, or the epoch. Keep that state in file-static storage in `cart.c` (one cartridge is loaded per process) and say so in a comment, or ask the lead to add the fields. Do not silently drop the RTC, and do not stuff the epoch into `ram`.

## Work order

1. Re-read `cart.c`, then finish `cart_load`: copy the ROM, parse `cart_type`, `rom_size_code`, `ram_size_code` into the frozen fields, set `mbc` via the table, set `battery`/`has_rtc`, compute `rom_size`/`ram_size` with the provided size functions, allocate RAM, validate the logo and checksum into `logo_ok`/`header_checksum_ok`, and load `<rom>.sav`. Check `gbemu --rom x.gb --info` against the file size for five ROMs.
2. ROM-only path (MBC_NONE): reads are bank 0, writes to `0x0000-0x7FFF` ignored, RAM at `0xA000` when present. Confirm a 32 KiB cart still boots.
3. MBC1 ROM banking at `0x2000-0x3FFF`, including the zero-to-one fixup and the size mask. Run `gbemu_tests m08_mbc1_bank0_quirk`.
4. MBC1 mode select at `0x6000-0x7FFF` and the upper bits at `0x4000-0x5FFF`, plus RAM banking (`0xA000`) and RAM enable. Run `m08_mbc1_mode_ram`.
5. MBC3: RAM/RTC enable, 7-bit ROM bank, RAM bank 0-3, RTC register select 0x08-0x0C, latch via `0x6000-0x7FFF`. Make time injectable - derive the counters from a stored epoch plus the host clock at load; never call `time()` from `cart_read`. Run `m08_mbc3_rtc`.
6. MBC5 9-bit banks and MBC2's nibble RAM. Run `m08_mbc5_9bit`.
7. Battery: `ram_dirty` on RAM writes, `cart_save` writing exact `ram_size` bytes, `cart_load` restoring them. Run `m08_battery_save_roundtrip` against a synthetic fixture, then against a real battery game.
8. Guards: assert on any ROM offset past `rom_size` in `cart_read`, on RAM access when `ram_size == 0`, and log every bank-register write behind a debug flag.

## Acceptance tests

```
gbemu_tests m08_                      # add tests/t_m08_cart.c; expect "ALL GREEN", exit 0
gbemu --rom <rom>.gb --info           # provided report; must show the right MBC and sizes
gbemu --rom <battery-game>.gb --frames 600 --headless    # make an in-game save, exit
gbemu --rom <battery-game>.gb --frames 600 --headless    # the save must still load
```

Tests the student must write: `m08_mbc1_bank0_quirk`, `m08_mbc1_mode_ram`, `m08_mbc3_rtc`, `m08_mbc5_9bit`, `m08_battery_save_roundtrip`.

Pass criteria: `gbemu_tests m08_` prints `ALL GREEN` and exits 0; `--info` matches the file size and header for five hand-checked ROMs; the `.sav` is exactly the header's RAM size and reloads to identical bytes; a real battery game's progress survives exit and restart, with the ROM name recorded in your notes.

## Common traps

- MBC1 writing 0 to the ROM bank register. Symptom: the bank switch lands in the header area and the game hangs or shows garbage. Cause: bank 0 is not directly selectable; the hardware substitutes 1. Detect: `m08_mbc1_bank0_quirk`; log each bank write with the resulting effective bank.
- MBC1 mode 0 versus mode 1. Symptom: multi-megabyte ROMs execute the wrong code, or RAM banking is ignored so a save corrupts. Cause: applying `bank_hi` to both the low ROM window and the RAM bank at once, or remapping `0x0000-0x3FFF` in mode 1. Detect: `m08_mbc1_mode_ram`.
- Header size versus file size. Symptom: reads past the end of `rom` - garbage now, occasional segfault later. Cause: using the file length for banking or trusting a `rom_size_code` that does not match. Detect: assert `offset < rom_size` in `cart_read`, and compare `cart_rom_size_bytes(rom_size_code)` against the file in `--info`.
- RAM enable check too strict or too loose. Symptom: saves never land, or writes leak into RAM while the game thinks it is disabled. Cause: requiring the whole byte to equal 0x0A instead of its low nibble, or never storing the disabled state. Detect: log enable transitions; mooneye's MBC ROMs.
- MBC3 latch done on a single write. Symptom: the in-game clock freezes or jumps; reads return live counters mid-update. Cause: the latch requires 0x00 then 0x01 to `0x6000-0x7FFF` in that order, and reads must return the latched copy. Detect: `m08_mbc3_rtc` with two latches around an injected time jump.
- RTC registers on a cart without an RTC. Symptom: the game hangs on its clock setup screen, or reads RAM where it expects a counter. Cause: returning live RAM for 0x08-0x0C when `has_rtc` is false (hardware returns 0xFF), or selecting the RTC when the register is below 0x08. Detect: fixtures for type 0x0F versus 0x13.
- MBC5 given MBC1's zero-to-one rule. Symptom: an MBC5 title screen crashes exactly when it switches to bank 0. Cause: copying the MBC1 fixup, and dropping bit 8 which lives at `0x3000-0x3FFF`, not `0x4000`. Detect: `m08_mbc5_9bit` with a >512 KiB fixture.
- Unrecognised type taken as ROM-only without a word. Symptom: an obscure cart boots, then misbehaves in a way nothing explains. Cause: the `default:` case doing nothing. Detect: warn once with `cart_type` and `cart_mbc_name()`; the `--info` report must say `UNKNOWN` or `MBC_OTHER` out loud.

## Hint ladder

### H1

- Which header byte (`rom_size_code`) tells you how many 16 KiB ROM banks exist, and what does the hardware do when a game writes a bank number larger than that count?
- Which MBC lets you select bank 0, and which one does not? What is the one-sentence difference between MBC1 mode 0 and mode 1?
- MBC3 selects between four RAM banks and five RTC registers through the same 8-bit register at `0x4000-0x5FFF`. What distinguishes the two cases, and what should a read return when neither is present?

### H2

- Technique: build fixture cartridges in memory with `t_build_rom`, change `rom[0x0147]`/`rom[0x0148]`/`rom[0x0149]` for the chip and size you want, and fill bank k with the byte k. Four of the five tests can be written this way, with no ROM files at all. Only the battery round trip needs a file on disk.
- Spec pointers: docs/03 3.4/3.5 for the tables and register maps, Pan Docs per-chip pages for the quirks, mooneye's per-case MBC ROMs for confirmation.
- Debug technique: add a flag that logs every write to `0x0000-0x7FFF` with the register name, the raw value and the resulting effective bank. Almost every MBC bug is visible in that log within ten lines.
- Persistence technique: write to a temporary file and rename over the `.sav`, so a crash never truncates a real save.

### H3

- `cart_ram_size_bytes()` and `cart_rom_size_bytes()` are the truth; `rom_size_code` is an exponent for ROM (32768 << code) and an enumeration for RAM (0, 2 KiB, 8 KiB, 32 KiB, 128 KiB, 64 KiB).
- RAM enable is `(value & 0x0F) == 0x0A`; anything else disables. MBC2 instead uses address bit 8 of `0x0000-0x3FFF`: clear selects RAM enable, set selects the ROM bank. Its RAM is 512 bytes of 4-bit nibbles, reads return the nibble OR 0xF0, writes keep only the low nibble, and only `0xA000-0xA1FF` is mapped.
- RTC registers 0x08..0x0C are seconds, minutes, hours, days-low, days-high (bit 0 is the day counter's bit 8, bit 6 halt, bit 7 carry). Derive the counters from a stored epoch and latch them into a shadow copy on the 0x00-then-0x01 write.
- MBC5's rumble bit is bit 3 of `0x4000-0x5FFF`; it must not leak into the RAM bank number.

## Done when

- `gbemu_tests m08_` reports 5/5 passing and prints `ALL GREEN`.
- `--info` prints the correct MBC name and bank counts for at least five real ROMs (one per family, one no-MBC).
- `m08_battery_save_roundtrip` proves the `.sav` is exactly `ram_size` bytes and reloads byte-identically.
- A real battery game's save survives exit and restart, with the ROM name and what you verified written down.
- Every unmapped cartridge type maps to MBC_OTHER and produces one clear warning; no file size or header disagreement is silently accepted.

## Stretch

- MMM01 support, or an explicit documented refusal: it uses a start-up sequence and a multi-cart layout that no test here needs.
- Persistent wall-clock RTC: keep the host epoch in a sidecar `<rom>.rtc` so `<rom>.sav` stays exactly `ram_size` bytes, then advance the counters on load.
- MBC1 multicart behaviour: the `0x20/0x40/0x60` upper-bank values in mode 0, and check that a 1 MiB MBC1 ROM still reaches every bank.

## Commit

`git commit -am "cart: MBC1/2/3/5 banking, RTC latch, battery .sav persistence"`
