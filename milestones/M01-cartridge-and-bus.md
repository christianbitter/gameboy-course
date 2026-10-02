# M01 - Cartridge and bus

**Goal** - Load a `.gb` file, parse its header into `cart_t`, and build the address-decoder skeleton that every later milestone hangs off: ROM, VRAM, WRAM and its echo, OAM, I/O storage, HRAM, IE, and the loud `0xFF` for everything unmapped.

**Estimated effort** - 6-9 h over 1-2 sessions (header ~1 h, decode chain ~2 h, debugging the echo/wrap arithmetic ~2 h).

## Read first

- `docs/03-memory-and-cartridge.md` sections 3.1-3.4 and 3.7. It owns the memory map, the header offset table, the checksum formula, the ROM/RAM size code tables and the bus-trap list. Keep it open.
- `gb/include/gb/cart.h` and `gb/include/gb/bus.h` - the structures and signatures are provided and frozen.
- `gb/include/gb/gb.h` - `struct gb_s` (you fill `gb.cart` and `gb.bus` through these functions only).
- `gb/include/gb/gb.h` for `gb_read_file()` and `gb_load_rom_file()` (given): note who owns the file buffer and who owns `cart.rom`.
- `reference/cheatsheet-opcode-map.md` - only as a reminder that nothing here is about opcodes.
- Pan Docs (linked from docs/03): "Memory Map", "The Cartridge Header", "External Memory and Hardware", "Echo RAM", "FEA0-FEFF range", "OAM DMA Transfer".

## Why this milestone exists

The bus is the only path from the CPU to the rest of the machine, so a decode bug here presents as a CPU bug, a PPU bug, or a hang, three milestones later. M01 is also where the ROM-only cartridge stops being a file and becomes an address range, which is the mental model MBC1 needs in M08: a bank is a *window*, and a window is just a branch in the decoder. If you skip the echo-RAM mirror, the WRAM upper-page handling, or the `0xFF` default, you will not notice for weeks: test ROMs rarely touch `E000-FDFF`, but a surprising number of games use it as scratch, and every one of them fails as "a random register got corrupted". Finally, header parsing is the one place where a "warn, never reject" policy matters: homebrew, hacks and test ROMs violate the logo and checksum constantly, and an emulator that refuses them is an emulator you cannot test with.

## Deliverable contract

`gb/src/cart.c`:

```c
bool cart_load  (gb_t *gb, const u8 *data, size_t size, const char *path);
void cart_unload(gb_t *gb);
u8   cart_read  (gb_t *gb, u16 addr);
void cart_write (gb_t *gb, u16 addr, u8 value);
void cart_save  (gb_t *gb);       /* write <path>.sav only if ram_dirty */
```

`cart_load` **copies** `data` into a buffer it owns and stores it in `gb->cart.rom`; the caller frees its own file buffer. It must fill, at minimum: `rom`, `rom_size`, `title`, `cart_type`, `rom_size_code`, `ram_size_code`, `mbc`, `battery`, `has_rtc`, `header_checksum_ok`, `logo_ok`, `ram`, `ram_size`. `ram` is `malloc`'d from the *header* RAM size (NULL when the header says none) - not from the file size, and not a fixed array.

`gb/src/bus.c`:

```c
u8   bus_read   (gb_t *gb, u16 addr);
void bus_write  (gb_t *gb, u16 addr, u8 value);
u16  bus_read16 (gb_t *gb, u16 addr);   /* two 8-bit reads, little endian */
void bus_write16(gb_t *gb, u16 addr, u16 value);
void bus_tick   (gb_t *gb, u32 tcycles);
```

Decode skeleton, one branch chain, no flat 64 KiB array:

```
  0000-00FF  boot ROM while gb->boot_rom_enabled, else cart    (check EVERY access)
  0000-3FFF  cart bank 0 / 4000-7FFF cart bank N              -> cart_read
  8000-9FFF  gb->bus.vram[addr & 0x1FFF]
  A000-BFFF  cart RAM                                          -> cart_read (M08 banking)
  C000-DFFF  gb->bus.wram[addr - 0xC000]
  E000-FDFF  gb->bus.wram[addr - 0x2000 - 0xC000]     <- the mirror
  FE00-FE9F  gb->bus.oam[addr - 0xFE00]
  FEA0-FEFF  unmapped default
  FF00-FF7F  gb->bus.io[addr - 0xFF00]   (BUT: FF00/FF01/FF02/FF04-FF07/FF0F/FF40-FF4B
                                          have live owners; see the ownership table)
  FF80-FFFE  gb->bus.hram[addr - 0xFF80]
  FFFF       gb->bus.ie
  default    read -> 0xFF, write -> ignored
```

Also in M01: writing `FF46` (DMA) performs the 160-byte copy `memcpy(gb->bus.oam, src, 0xA0)` immediately with `src = value << 8`; the 160-cycle stall is M09.

`bus_tick(gb, t)` is the clock distributor: `gb->total_ticks += t`, then `timer_tick`, `ppu_tick`, `serial_tick`. Implement the calls now even though those components are stubs - it is the seam `docs/01-orientation.md` section 1.2 warned you not to break. `bus_request_interrupt()` is already given (`gb/src/gb.c`); do not define it here.

Test-facing facts:

| Test | What it must be able to read |
| --- | --- |
| `m01_header_parse` | `gb->cart.title`, `cart_type`, `rom_size_code`, `ram_size_code`, `battery`, `mbc` after `cart_load` |
| `m01_checksum` | `gb->cart.header_checksum_ok`, plus `cart.rom[0x0134..0x014C]` vs `cart.rom[0x014D]` |
| `m01_cart_read_rom` | `cart_read(gb, 0x0000..0x7FFF)` for a `MBC_NONE` cart |
| `m01_bus_wram_echo` | write `C000+i`, read `E000+i` (and the reverse) |
| `m01_bus_unmapped_ff` | `bus_read` of any address with no owner returns `0xFF` |
| `m01_bus_hram_ie` | `FF80-FFFE` round-trips; `FFFF` is `gb->bus.ie` |

## Work order

1. `cart_load`: validate size only (reject a file too small to contain a header, or a declared ROM size that is nonsense). Copy the data, then parse offsets `0x0134-0x014F` into the named fields. Run `.\gb\build.cmd -Test m01_header_parse`.
2. ROM/RAM size codes -> `rom_size_bytes`-equivalent logic and the `ram` allocation. Use the tables in docs/03 section 3.4. Run `m01_header_parse` again; it must now cover both a 32 KiB ROM-only cart and a bigger MBC cart.
3. Checksum and logo: compute both into `header_checksum_ok` / `logo_ok`. **Reject nothing.** Run `m01_checksum`.
4. `cart_read` for `MBC_NONE`: `0000-3FFF` -> `rom[addr]`, `4000-7FFF` -> `rom[addr]`. Guard against reading past `rom_size` (return `0xFF`). Run `m01_cart_read_rom`.
5. `cart_write` for `MBC_NONE`: writes to `0000-7FFF` are discarded; writes to `A000-BFFF` go to `ram` and set `ram_dirty`. Do not fall through to a generic array here.
6. `bus_read`/`bus_write` chain: ROM/RAM first, then VRAM, WRAM, echo, OAM, I/O, HRAM, IE, `default -> 0xFF`.
7. Echo RAM. Then immediately test the two directions: write `C000+D`, read `E000+D`; write `E000+D`, read `C000+D`. Run `m01_bus_wram_echo`.
8. HRAM and IE. `FF80-FFFE` is ordinary storage; `FFFF` is `gb->bus.ie`. Run `m01_bus_hram_ie`.
9. Unmapped default. Check `FEA0`, `FF03`, `FF08-FF0E`, `FF15`, `FF1F`, `FF27-FF2F`, `FF4C-FF7F`. Run `m01_bus_unmapped_ff`.
10. `bus_read16`/`bus_write16` as two 8-bit accesses at `addr` and `addr+1` - never a single decode, which is wrong at bank edges and on I/O.
11. `bus_tick` wiring and `FF46` instant DMA. Run the whole filter: `.\gb\build.cmd -Test m01`.

## Acceptance tests

```
.\gb\build.cmd -Test m01
.\gb\build.cmd -Test m01_bus_wram_echo     # one at a time while debugging
.\gb\build\gbemu.exe --rom <any .gb> --info
```

Pass criterion: `m01_header_parse`, `m01_checksum`, `m01_cart_read_rom`, `m01_bus_wram_echo`, `m01_bus_unmapped_ff`, `m01_bus_hram_ie` all PASS; `--info` prints a title and sizes for a real ROM without aborting; and a deliberately corrupted copy of that ROM (flip one byte at `0x014D`) still loads, with the checksum reported as bad.

## Common traps

- Echo writes going nowhere. Symptom: `m01_bus_wram_echo` fails in one direction only. Cause: the mirror arm implemented with a wrong base (`addr + 0x2000` instead of `addr - 0x2000`), or a mask applied only to reads. Detect: test write-through in both directions at `C000`, `DFFF`, `FDFF`.
- `0x00` from unmapped reads. Symptom: a game decides it is a different hardware revision, or `m01_bus_unmapped_ff` fails. Cause: defaulting an unclaimed read to `0x00`, or `memset`ing `bus_t` and assuming "uninitialised means unmapped". Detect: `bus_read` an address you know you never decode; it must be `0xFF`. Note the one honest exception: real DMG hardware returns `0x00` in `FEA0-FEFF` outside OAM-block windows (`docs/03` section 3.2), and the course's unmapped rule is `0xFF`. Record the discrepancy in `NOTES.md`; do not "fix" it before M09.
- `cart_load` storing the caller's pointer. Symptom: garbage header fields, or a crash a few million instructions in, after the loader's buffer is freed. Cause: `cart->rom = (u8*)data` instead of a copy. Detect: run under a debugger/ASan-equivalent, or assert `cart->rom != data`.
- RAM size taken from anywhere but the header. Symptom: MBC carts read `0xA000` from the ROM table, or RAM writes are silently dropped. Cause: allocating `ram` from `size - 0x8000` or skipping the allocation when `ram_size_code == 0`. Detect: `m01_header_parse` on a cart whose file is padded/truncated relative to the declared sizes; compare `cart.ram_size` against docs/03's table.
- Shared `bus_read16` decode. Symptom: `LD A,(nn)` at a bank edge (`3FFF/4000`) or on `FFxx` returns one correct and one wrong byte. Cause: implementing `bus_read16` as `*(u16*)&flat[addr]`, or as a single decode of the high byte. Detect: assert `bus_read16(a) == bus_read(a) | bus_read(a+1) << 8` at `0x3FFF` and `0xFF0F`.
- Non-terminated title. Symptom: `--info` prints the title followed by garbage, or a crash inside `printf` reading past the struct. Cause: copying 16 bytes into `char title[17]` without writing `title[16] = '\0'`; a 16-character title leaves the last byte uninitialised. Detect: load a synthetic header whose 16 title bytes are all non-zero and assert `strlen(gb->cart.title) <= 16`.
- I/O treated as a plain array in M01. Symptom: `FF04` reads as `0x00` forever and later the timer tests fail for "no reason". Cause: `bus_read` returns `io[0x04]` instead of `gb->timer.div_counter >> 8`. Detect: after `timer_tick` exists, assert `bus_read(FF04) != io[4]`; write down the ownership table in `NOTES.md` now so M05 cannot surprise you.
- Enforcing the logo or checksum. Symptom: some test ROMs and homebrew refuse to load. Cause: `cart_load` returning false on a logo mismatch. Detect: `logo_ok` and `header_checksum_ok` are *reports* for `--info`; the only legal rejections are a file too small for a header and an impossible size. Never enforce the `0x0104-0x0133` logo check yourself - on real hardware the boot ROM does it (`docs/03` section 3.3), and you are skipping the boot ROM.

## Hint ladder

### H1

- What is the *window* a cartridge decoder selects, in bytes, and why does the answer make "ROM is a file" the wrong model?
- For `E000-FDFF`, which address arithmetic maps `E000` to `C000` and `FDFF` to `DDFF`, and what does that same arithmetic do to `FF00` if you apply it too broadly?
- Which I/O addresses in `FF00-FF7F` are *not* storage, and who owns each of them in `struct gb_s`? Answer before you write the I/O arm.

### H2

- Technique: write the decode as a small ordered `if` chain with comments naming the range endpoints, not as nested `switch` cases on the high byte. The tracer will print addresses, and you want to be able to read the chain against those numbers.
- Technique: build the M01 tests' own cart images in C (`u8 rom[32768] = {0}; rom[0x0147] = 0x00; ...`) and pass them to `cart_load`. No ROM file needed for M01; that keeps the failure loop under a second.
- Technique for echo: implement one helper `wram_ptr(gb, u16 addr)` that returns NULL when the address is outside `C000-FDFF`, and call it from both read and write. Two call sites, one arithmetic bug surface.
- Spec pointers: docs/03 sections 3.2 (map + rules), 3.4 (header table, size codes, checksum), 3.6 (DMA), 3.7 (the exact traps list this milestone exists to avoid).

### H3

- Chip select for echo: `offset = addr & 0x1FFF` is correct for both `C000-DFFF` and `E000-FDFF` with an 8 KiB `wram`. `D000`/`D020` and `F000`/`F020` alias only if you apply the mask to the wrong half.
- HRAM: `hram[addr & 0x007F]` for `FF80-FFFE` (127 bytes; `FFFF` is IE and is *not* in HRAM). `FEA0-FEFF` is outside both and must reach your `default`.
- Header checksum (`0x014D`), arithmetic mod 256: `x = 0; for (i = 0x0134; i <= 0x014C; i++) x = (u8)(x - rom[i] - 1); /* rom[0x014D] == x */`. The logo is the 48 bytes at `0x0104-0x0133`; compare against the table in Pan Docs' header section and set `logo_ok`, then ignore it.
- `--info` is your first real oracle: if the printed title, ROM/RAM size and MBC name do not match the ROM you know, fix the parse before writing a single bus test.

## Done when

- `.\gb\build.cmd -Test m01` reports all six `m01_` tests PASS.
- `gbemu --rom <real>.gb --info` prints a sane title and the declared ROM/RAM sizes for three different carts, including one MBC cart, without aborting.
- Writing and reading through `E000-FDFF` is indistinguishable from `C000-DDFF` in both directions, at the range ends and in the middle.
- Every unclaimed address returns `0xFF`, and the `FEA0-FEFF` hardware-`0x00` discrepancy is recorded in `NOTES.md`.
- `bus_read16` at `0x3FFF` equals `bus_read(0x3FFF) | (bus_read(0x4000) << 8)`.
- `NOTES.md` contains the register-ownership table from M00 updated with "FF46 -> bus_t dma_*" and the M01 commit is in `git log`.

## Stretch

- Print a hexdump of the 80-byte header next to your parsed fields in `--info` and diff the two by eye once per new ROM. It catches off-by-one field offsets in seconds.
- Implement `cart_save`/save-load round trip (`<rom>.sav`) now, even though M01 has no battery carts, and add a hand-written test that dirties RAM and reloads it. M08 will be ready for it.
- Add a `--bus-trace` counter: increment per range in `bus_read`/`bus_write`, print the histogram on exit (`HRAM: 1204, WRAM: 88113, unmapped: 3`). Three unmapped reads in a real game is a bug report.

## Commit

```
feat(bus,cart): ROM-only address decode, echo RAM, header parse, checksum report (M01 green)
```
