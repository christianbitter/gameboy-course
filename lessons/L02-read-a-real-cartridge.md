# L02 — Read a real cartridge

**Time** — theory ~20 min | coding ~60 min | verify ~10 min
**You will end with** — your program reading an actual `.gb` file, printing its
title, its cartridge type and its sizes, and serving bytes from ROM to any address
the CPU asks for. AHA #2: *that is real hardware data, parsed by my code.*
**Tests that must go green** — `m01_header_parse`, `m01_checksum`,
`m01_cart_read_rom` (the other three `m01_*` tests belong to L03).
**Depends on** — L01. You will edit `gb/src/cart.c` only.

---

## 1. Read this — the whole theory for today

### The cartridge is a memory chip with a header

A cartridge is ROM (and optionally RAM) wired to the address bus. Because ROM
cannot be written, everything interesting happens by *snooping*: when software
writes to `0x0000-0x7FFF`, the memory bank controller (MBC) does not store
anything — it loads a **bank number** into a latch, and the latch changes which
16 KiB of ROM appears at `0x4000-0x7FFF`. Call that the "snooped write" model and
hold on to it; banking itself is L26.

Today the cartridge is simpler: a header to parse and bytes to serve.

### The header

| Offset | Field | Notes |
| --- | --- | --- |
| `0100-0103` | entry point | `NOP; JP 0150` in real cartridges |
| `0104-0133` | Nintendo logo | 48 bytes; the *boot ROM* checks it, not you |
| `0134-0143` | title | ASCII, zero-padded |
| `0143` | CGB flag | `0x80` = CGB+DMG, `0xC0` = CGB only |
| `0144-0145` | new licensee | two ASCII characters |
| `0147` | **cartridge type / MBC** | `0x00` ROM-only, `0x01-03` MBC1, ... |
| `0148` | **ROM size code** | `0x00` = 32 KiB, `0x01` = 64 KiB, ... `0x08` = 8 MiB |
| `0149` | **RAM size code** | `0x00` none, `0x02` 8 KiB, `0x03` 32 KiB, ... |
| `014D` | header checksum | see below |
| `014E-014F` | global checksum | often wrong in the wild; ignore it |

### The header checksum — get this exactly right

```
x = 0
for each byte b from 0x0134 to 0x014C inclusive:
    x = x - b - 1          /* all arithmetic mod 256 */
/* x must equal rom[0x014D] */
```

Note the shape: a running subtraction, not an addition, and the `- 1` per byte.
`0x0134` to `0x014C` is 25 bytes: title (16) + CGB flag + licensee (2) + SGB +
type + rom size + ram size + destination + old licensee + version.

### Validation policy: warn, never reject

Homebrew breaks the rules: missing or wrong logo, zero checksum, a declared ROM
size that does not match the file. If you reject those, you reject real ROMs.
So: report, do not fail. The two things worth refusing to continue with are a file
too small to contain a header, and nothing else — even the size mismatch is a
warning.

### Sizes come from the header, not the file

`--info` and the bank count must use the header tables. The provided helpers
`cart_rom_size_bytes(code)` and `cart_ram_size_bytes(code)` in `gb/src/cart.c`
already implement them, and `cart_print_info()` is written for you.

> **What matters for the code you are about to write**
>
> * The checksum loop is `x = x - byte - 1` over `0x0134..0x014C`, compared
>   against `rom[0x014D]`. Store the *result*, not the raw byte, in
>   `gb->cart.header_checksum_ok`.
> * `cart_load` **copies** the ROM into `gb->cart.rom` (it owns it;
>   `cart_unload` frees it). The caller frees its own file buffer.
> * Allocate `gb->cart.ram` from `ram_size_code`, and set `gb->cart.ram_size`
>   to the header's value — `m01_header_parse` checks both.
> * The two calls the tests make are `cart_load(gb, data, size, path)` and, for
>   reads, the bus — so make `cart_read` correct for `0000-7FFF` today.

---

## 2. Your task — 60 min

Work in `gb/src/cart.c`. The stubs and their TODO comments are already there.

1. **Copy the ROM** (5 min). `malloc(size)`, `memcpy`, set `rom_size`. Return
   `false` with a message only if the file is smaller than `0x0150`.
2. **Parse the header** (20 min). Fill these fields, because the tests read them:
   `title`, `cart_type`, `rom_size_code`, `ram_size_code`, `mbc`
   (`MBC_NONE` for `0x00`, `MBC_1` for `0x01-0x03`, `MBC_2` for `0x05-0x06`,
   `MBC_3` for `0x0F-0x13`, `MBC_5` for `0x19-0x1E`, `MBC_OTHER` otherwise), and
   `battery` (`0x03`, `0x06`, `0x09`, `0x0D`, `0x0F`, `0x10`, `0x13`, `0x1B`,
   `0x1E`, `0x22`, `0xFF`).
   **`logo_ok` is optional today** — no test reads it, and it only feeds the
   `--info` report. Either compare `0x0104-0x0133` against the 48-byte logo (Pan
   Docs' header page, linked in `reference/external-references.md`), or set
   `logo_ok = false` with a `TODO(L26)` and move on. Do not let a 48-byte constant
   eat your 90 minutes.
3. **The checksum** (5 min). Implement the loop above; set
   `header_checksum_ok`.
4. **Allocate RAM** (5 min). `gb->cart.ram = calloc(1, cart_ram_size_bytes(code))`
   when the size is non-zero; `ram_size` = that value.
5. **`cart_read`** (10 min). For `0x0000-0x3FFF` return `rom[addr]`. For
   `0x4000-0x7FFF` return `rom[addr]` too — with `MBC_NONE` the upper half is just
   a mirror of a 32 KiB ROM. Banking replaces this line in L26.
6. **`cart_write`** (10 min). Writes to `0x0000-0x7FFF` are **ignored** (this is
   where MBC commands will go later). Writes to `0xA000-0xBFFF` go into
   `gb->cart.ram` if it exists and `ram_enabled` is true — for a `MBC_NONE`
   cartridge, treat RAM as always enabled.
7. **Run the tests** (5 min):
   ```
   .\gb\build.cmd -Test m01
   ```
   Expect **exactly 3 green** (`m01_header_parse`, `m01_checksum`,
   `m01_cart_read_rom`) and 3 still red (`m01_bus_*` — those are L03). If a test
   you expected to pass is red, read its name: the message tells you which field
   you did not set.
8. **Try it on a real file** (optional, 10 min if you already have a ROM):
   ```
   .\gb\build\gbemu.exe --rom roms\<something>.gb --info
   ```
   Cross-check the printed title against a hexdump of `0x0134`, and the size
   against the file length. `roms/README.md` has sources for free ROMs.

---

## 3. Prove it — 10 min

```
.\gb\build.cmd -Test m01
```
```
[       OK ] m01_cart_read_rom
[  FAILED  ] m01_bus_unmapped_ff
             src/bus.c:31: UNIMPLEMENTED ...
[       OK ] m01_checksum
[       OK ] m01_header_parse
...
```
Green on the three `cart`/`header`/`checksum` tests, red on the three `bus_*` ones,
and the run finishes. That is success for L02.

---

## 4. If it fails

| Symptom | Cause | Find it |
| --- | --- | --- |
| `m01_header_parse`: title is garbage or empty | you read the title from the wrong offset, or forgot to NUL-terminate `title[17]` | compare your offset against the table; `0x0134`, 16 bytes |
| `m01_checksum` fails on a valid ROM | classic: `x = x + b + 1`, or the loop ran over 26 bytes including `0x014D` | the range ends **at** `0x014C`; the operator is `-` |
| `header_checksum_ok` is true for a corrupted header | you compared against the wrong byte, or never stored the result | the second half of `m01_checksum` deliberately corrupts `0x0134` and expects `false` |
| `cart_load` returns false on a good ROM | you added a logo or checksum rejection path | warn, never reject; only a too-small file is fatal |
| `m01_cart_read_rom` reads zeros | `bus_read` is still a stub, so the test never reaches `cart_read` | expected until L03 — but check that `cart_read` itself is right by calling it directly in a scratch test |

---

## 5. Done when

- [ ] `m01_header_parse`, `m01_checksum`, `m01_cart_read_rom` are green
- [ ] `m01_bus_*` are still red, and you know that is L03's job
- [ ] `--info` on a real ROM prints a title that matches a hexdump
- [ ] You can write the checksum loop from memory
- [ ] You can explain why a wrong header checksum must not stop the emulator
- [ ] Commit message like `feat(cart): parse header, checksum, ROM-only reads (L02)`

---

## 6. Optional, only if you have time

* `docs/03-memory-and-cartridge.md` §3.3-3.4 — the boot ROM's role in the logo
  check, and why skipping the boot ROM is the normal choice.
* Add `--info` output for the CGB flag and the global checksum to your `NOTES.md`
  for later reference.

Next: **[L03 — The address decoder](L03-the-address-decoder.md)**
