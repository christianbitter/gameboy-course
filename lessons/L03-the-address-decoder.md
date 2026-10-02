# L03 — The address decoder

**Time** — theory ~20 min | coding ~60 min | verify ~10 min
**You will end with** — `bus_read`/`bus_write` serving the entire 64 KiB map, so
anything in the machine can be reached through one pair of functions. AHA #2
continued: *the CPU's whole world is now mine.*
**Tests that must go green** — `m01_bus_wram_echo`, `m01_bus_unmapped_ff`,
`m01_bus_hram_ie` (all six `m01_*` should be green after this).
**Depends on** — L02. You will edit `gb/src/bus.c` only.

---

## 1. Read this — the whole theory for today

### The map

| Range | Size | Contents | Who owns it |
| --- | --- | --- | --- |
| `0000-3FFF` | 16 KiB | ROM bank 0, **or the boot ROM while `FF50.0 == 0`** | `cart_read` |
| `4000-7FFF` | 16 KiB | switchable ROM bank | `cart_read` |
| `8000-9FFF` | 8 KiB | VRAM | `gb->bus.vram` |
| `A000-BFFF` | 8 KiB | cartridge RAM | `cart_read` |
| `C000-CFFF` | 4 KiB | WRAM bank 0 | `gb->bus.wram[0x0000]` |
| `D000-DFFF` | 4 KiB | WRAM bank 1 | `gb->bus.wram[0x1000]` |
| `E000-FDFF` | 7.5 KiB | **echo** of `C000-DDFF` | same bytes, offset `-0x2000` |
| `FE00-FE9F` | 160 B | OAM (40 sprites x 4 bytes) | `gb->bus.oam` |
| `FEA0-FEFF` | 96 B | prohibited: reads `0xFF`, writes ignored | — |
| `FF00-FF7F` | 128 B | I/O registers | see the ownership table |
| `FF80-FFFE` | 127 B | HRAM | `gb->bus.hram` |
| `FFFF` | 1 B | `IE` | `gb->bus.ie` |

### Three rules ROMs actually depend on

1. **Unmapped reads return `0xFF`, not `0x00`.** Some ROMs probe the map to guess
   the hardware revision. `FEA0-FEFF` is the range this matters for today.
2. **Echo RAM is a real mirror.** `bus_read(0xE123)` must equal
   `bus_read(0xC123)`, and a write to `0xE456` must land in WRAM at `0xC456`.
   The offset is exactly `0x2000`.
3. **I/O reads have side effects.** Never cache a value read from `FF00-FF7F`:
   `LY`, `DIV`, `TIMA` and `P1` change under you. Today most of those registers are
   plain storage, and that is fine — just do not build anything that assumes a
   cached copy is still valid.

### 16-bit accesses are two 8-bit accesses

```c
u16 bus_read16(gb, addr)  ->  lo = bus_read(gb, addr); hi = bus_read(gb, addr+1);
                              return lo | (hi << 8);
```

Never cast an address into a flat array and read two bytes at once: at a bank edge
that would read the wrong bank, and at `FF00` it would skip the side effects.

### The register-ownership table (from `gb/include/gb/bus.h`)

Every I/O register has exactly **one** owner, and `bus_read`/`bus_write` are the
only code that translates an address into that owner's field:

```
FF00            joypad_t.select        via joypad_read(gb)          -> L19
FF01/FF02       serial_t.sb / .sc                                   -> L17
FF04-FF07       timer_t (DIV is DERIVED from timer_t.div_counter)   -> L16
FF0F  IF        bus.io[0x0F]           macros GB_IF(gb) / GB_IE(gb)
FF40-FF4B       ppu_t fields (LY read-only)                         -> L04/L18/L23
FF46  DMA       bus_t dma_* fields                                  -> L30
FF50            gb->boot_rom_enabled
FFFF  IE        bus.ie
everything else bus.io[]
```

That is why there is no separate `if_` field anywhere: `IF` is just
`gb->bus.io[0x0F]`, reached through `GB_IF(gb)`.

### The boot ROM, in one paragraph

`0000-00FF` is normally ROM bank 0, except while the boot ROM is mapped
(`FF50` bit 0 == 0). This course skips the boot ROM (`--no-boot-rom`, the default),
so `gb->boot_rom_enabled` starts false and the cartridge answers at `0000`. The
one consequence: `bus_read` must consult that flag on **every** access, because a
write to `FF50` unmaps the boot ROM mid-instruction. L18 fills in the post-boot
register values.

> **What matters for the code you are about to write**
>
> * Echo RAM: `addr - 0x2000` for `E000-FDFF`, on **both** read and write.
> * `FEA0-FEFF`: read `0xFF`, ignore writes.
> * `FFFF` is `IE` (`gb->bus.ie`), and `FF80-FFFE` is HRAM — `FFFE` is the last
>   HRAM byte, `FFFF` is not.
> * `bus_read16`/`bus_write16` must call the 8-bit functions, in low-then-high
>   order.
> * Route `FF00-FF7F` through a `switch` or an `if`-chain with a **named case per
>   register group**, so the later lessons have a place to plug in. For registers
>   with no owner yet, `gb->bus.io[offset]` is the correct holding pen.
> * Writing `FF46` starts OAM DMA: copy 160 bytes from `value << 8` to `FE00`.
>   Do it **instantly** today; the 160-cycle stall is L30.

---

## 2. Your task — 60 min

Work in `gb/src/bus.c`. `bus_request_interrupt()` is provided; do not change it.

1. **ROM and cartridge RAM** (5 min). `0000-7FFF` -> `cart_read`; `A000-BFFF` ->
   `cart_read`. (Yes, cart handles both; banking lives behind it.)
2. **VRAM, WRAM, OAM, HRAM** (10 min). Straight array indexing. Get WRAM's two
   4 KiB banks right: `C000-CFFF` is `wram[0]..wram[0xFFF]`, `D000-DFFF` is
   `wram[0x1000]..wram[0x1FFF]`.
3. **Echo RAM** (5 min). Both directions, offset `0x2000`.
4. **The prohibited range** (3 min). Read `0xFF`, ignore writes.
5. **`IE`** (3 min). `0xFFFF` <-> `gb->bus.ie`.
6. **I/O range** (20 min). A `switch` on the low byte for `FF00-FF7F`:
   * `FF0F` -> `GB_IF(gb)` (which is `bus.io[0x0F]`), reads with bits 5-7 set;
   * `FF46` -> the DMA copy;
   * `FF50` -> `gb->boot_rom_enabled = (value & 1) != 0 ? false : true;`
     (writing a non-zero value unmaps the boot ROM);
   * `FF40-FF4B` -> `ppu_t` fields is **L04**; for now let them fall through to
     `bus.io[]`;
   * everything else -> `gb->bus.io[offset]`, and reads return `0xFF` if you
     prefer to leave the storage zeroed. Either choice is fine; be consistent.
7. **16-bit accessors** (7 min). Two 8-bit calls each, low byte first.
8. **Leave `bus_tick` alone** (0 min). Its stub aborts on purpose; nothing calls it
   until L18, because the CPU tests drive `cpu_step` directly.
9. **Run the tests** (5 min):
   ```
   .\gb\build.cmd -Test m01
   ```
   All six green.

---

## 3. Prove it — 10 min

```
.\gb\build.cmd -Test m01
```
```
   6 passed,    0 failed,    0 skipped  (73 tests registered)   <- for the m01 filter
ALL GREEN
```
Then the regression sweep — the earlier lessons must still be green:

```
.\gb\build.cmd -Test m0
```
Expect `m00_*` and `m01_*` green, `m02_*` onward red with `UNIMPLEMENTED`. That is
exactly right: those are L05 onward.

---

## 4. If it fails

| Symptom | Cause | Find it |
| --- | --- | --- |
| `m01_bus_wram_echo` fails only on the `E000` direction | you handled echo on read but not write (or the offset sign flipped) | writes to `E000-FDFF` must land at `addr - 0x2000` |
| `m01_bus_wram_echo` reads `0x00` from `FDFF` | your echo range ends at `FDFF` but you wrote `- 0x2000` for a `DFFF` write that needs `+ 0x2000` | `DFFF` and `FDFF` are the same byte: 47 lines up |
| `m01_bus_unmapped_ff` gets `0x00` | you routed `FEA0-FEFF` to a zeroed array | return `0xFF` explicitly |
| `m01_bus_hram_ie` fails on `FFFE` | your HRAM range is off by one and swallowed `FFFF` | HRAM is `FF80-FFFE` (127 bytes); `FFFF` is IE |
| Writes "work" but reads return `0xFF` for everything | your `switch` falls through to the unmapped case | add the default case last, and check the `case` values are IO offsets, not full addresses |
| A later test breaks that used to pass | you changed `cart_read`/`cart_write` behaviour while wiring the bus | `.\gb\build.cmd -Test m0` shows the regression immediately; keep the bus a pure router |

---

## 5. Done when

- [ ] All six `m01_*` tests are green, and `-Test m0` shows no regression in `m00_*`
- [ ] `bus_read(0xE123)` equals `bus_read(0xC123)` in a scratch test you write and delete
- [ ] You can name the owner of `FF04`, `FF0F`, `FF4A` and `FFFF` without looking
- [ ] You can explain why `bus_read16` must not read two bytes at once
- [ ] Commit message like `feat(bus): full address decode, echo RAM, DMA copy (L03)`

---

## 6. Optional, only if you have time

* `docs/03-memory-and-cartridge.md` §3.2 and §3.7 — the map with the traps that
  only matter once games are running.
* Add a `--dump-mem` debug flag to `main.c` that hexdumps a range with
  `gb_hexdump` (already provided). Twenty minutes now, hours saved later.

Next: **[L04 — First pixels](L04-first-pixels.md)**
