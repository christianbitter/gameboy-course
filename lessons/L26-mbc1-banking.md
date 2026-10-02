# L26 — MBC1: banking

**Time** — theory ~25 min | coding ~55 min | verify ~10 min
**You will end with** — a cartridge that can show any of its ROM banks at
`0x4000-0x7FFF` and any of its RAM banks at `0xA000-0xBFFF`. This is the lesson that
turns "runs a 32 KiB ROM" into "runs a commercial game".
**Tests that must go green** — `m08_mbc1_bank0_quirk`, `m08_mbc1_mode_ram`
**Depends on** — L02/L03 (`cart_read`/`cart_write`, the bus). You will edit
`gb/src/cart.c`.

---

## 1. Read this — the whole theory for today

### The conceptual unlock, repeated because it is that important

A ROM cannot be written. So when software writes to `0x0000-0x7FFF`, nothing is stored:
the memory bank controller **snoops the write**. The **address range** is a command and
the **byte written** is its argument:

```
bus_write(0x2000, 0x03)   means   "put ROM bank 3 at 0x4000-0x7FFF"
                          NOT     "store 0x03 at address 0x2000"
```

If you have been treating `cart_write` as a store that gets discarded, that is exactly
right — and now it gets a meaning. Ten lines of `switch` is a whole MBC.

### MBC1's four command windows

| Address range | Command | Argument |
| --- | --- | --- |
| `0000-1FFF` | RAM enable | low nibble `0x0A` enables cartridge RAM; anything else disables it |
| `2000-3FFF` | ROM bank, low 5 bits | **writing 0 selects bank 1** |
| `4000-5FFF` | secondary register, 2 bits | meaning depends on mode |
| `6000-7FFF` | mode select | `0` = ROM banking mode, `1` = RAM banking mode |

And the bank arithmetic that makes it all work:

```
bank = value & 0x1F              /* only 5 bits exist in that register */
if (bank == 0) bank = 1          /* the quirk: bank 0 is unreachable here */
bank &= rom_bank_count - 1       /* mask to the size of this cartridge     */
```

Order matters, and it is not arbitrary: the substitution happens **before** the mask,
because the hardware substitutes on the 5-bit value it just latched. Get it backwards and
most cartridges still work — which is why the test checks both `0x00 -> 1` and
`0x1F -> 7` on an 8-bank ROM.

Bank 0 is permanently mapped at `0x0000-0x3FFF`; that mirror is what makes the RST and
interrupt vectors work no matter which bank is visible above.

### Modes: the secondary register means two different things

| Mode | `4000-5FFF` controls | RAM bank at `A000-BFFF` |
| --- | --- | --- |
| 0 (default) | upper bits of *ROM* banking | always bank 0 |
| 1 | the **RAM** bank (2 bits) | the selected bank |

MBC1 supports at most 2 MiB of ROM and 32 KiB of RAM, and those two limits are *why*
the register has to be shared: the hardware could not afford the pins for two
independent registers. So a game with 32 KiB of RAM switches to mode 1 to reach a RAM
bank, then back to mode 0 to reach a high ROM bank. That is exactly what
`m08_mbc1_mode_ram` walks through.

There is a further wrinkle for 512 KiB cartridges where the two registers are combined
into the ROM bank number. It affects a handful of games; note it, defer it, and check
`NOTES.md` for it if such a game misbehaves.

### RAM enable

`ram_enabled` gates all cartridge RAM access:

* when disabled, **writes to `A000-BFFF` are dropped**, and reads return garbage
  (modelling them as `0xFF` is the common choice);
* when enabled, reads and writes go to the currently selected RAM bank.

Games disable RAM when they are not using it, partly for power saving, so a game that
writes its save with RAM disabled loses the save — silently, from your side. The test
checks the disabled case first, which is why `m08_mbc1_mode_ram` starts by writing
`0x00` and asserting the write went nowhere.

> **What matters for the code you are about to write**
>
> * `cart_write` for `0x0000-0x7FFF` decodes **by address range**, and never stores. Put
>   the four ranges as four `else if` branches on the MBC type.
> * `cart_read` for `0x4000-0x7FFF` returns `rom[(rom_bank * 0x4000) + (addr - 0x4000)]`.
>   It must consult the current bank, not a cached pointer, because the bank can change
>   between two reads.
> * The bank-0 quirk is `if (bank == 0) bank = 1;` **after** masking to 5 bits and
>   **before** masking to the cartridge's bank count.
> * `rom_bank_count = rom_size / 0x4000`. The test's 128 KiB ROM therefore has 8 banks,
>   which is why `0x1F` must read back as bank 7.
> * `ram_enabled = ((value & 0x0F) == 0x0A)` — the low nibble, not the whole byte.
> * In mode 0 the RAM bank is **0** regardless of the secondary register; in mode 1 it is
>   the secondary register masked by `ram_bank_count`. `m08_mbc1_mode_ram` depends on
>   both halves of that sentence.
> * RAM is `ram_size` bytes from the header, split into 8 KiB banks; the bank index is
>   `ram_bank * 0x2000 + (addr - 0xA000)`.
> * `cart_read` must return `0xFF` (or any garbage) when RAM is disabled rather than
>   faulting: the test only asserts that the previously written value is *not* visible.

---

## 2. Your task — 55 min

Work in `gb/src/cart.c`.

1. **Bank bookkeeping** (10 min). Add `rom_bank_count` and `ram_bank_count` derived from
   the header sizes (or reuse the provided `cart_rom_size_bytes`/`cart_ram_size_bytes`
   plus the file size). Decide where the masking lives and write it in a comment.
2. **The four command windows** (20 min). One `switch` on `addr >> 13` (which maps
   `0x0000`, `0x2000`, `0x4000`, `0x6000` to 0, 1, 2, 3) is the cleanest shape, then a
   second `switch` on `gb->cart.mbc` inside. Only MBC1 and `MBC_NONE` matter today.
3. **`cart_read`** (20 min). Three ranges: bank 0 at `0x0000-0x3FFF`, the switchable bank
   at `0x4000-0x7FFF`, and cartridge RAM at `0xA000-0xBFFF` (with the enable check and
   the RAM bank offset).
4. **Run the tests** (5 min):
   ```
   .\gb\build.cmd -Test m08
   ```
   `m08_mbc1_bank0_quirk` and `m08_mbc1_mode_ram` green. The other three are L27/L28.

---

## 3. Prove it — 10 min

```
.\gb\build.cmd -Test m08
```
```
[       OK ] m08_mbc1_bank0_quirk
[       OK ] m08_mbc1_mode_ram
[  FAILED  ] m08_battery_save_roundtrip   <- L27
[  FAILED  ] m08_mbc3_rtc                 <- L28
[  FAILED  ] m08_mbc5_9bit                <- L28
```
Then the real test, because banking is the difference between a demo and a game:

```
.\gb\build\gbemu.exe --rom <a commercial MBC1 game>.gb --info
.\gb\build\gbemu.exe --rom <that game>.gb --frames 600 --dump-frame build\game.bmp
start build\game.bmp
```

`--info` first: confirm the cartridge type is MBC1 (`0x01-0x03`) and note the ROM size.
If the ROM is bigger than 32 KiB and you see a *blank* screen or an immediate hang,
banking is the first suspect — and the trace (`--trace build\t.txt --frames 2`) will
show `PC` jumping to an address whose contents are the wrong bank.

```
.\gb\build.cmd -Test m0
```
Regression sweep: nothing outside `m08` should change.

---

## 4. If it fails

| Symptom | Cause | Find it |
| --- | --- | --- |
| `m08_mbc1_bank0_quirk`: writing 0 keeps bank 0 | the substitution is missing, or applied after the mask | `if (bank == 0) bank = 1;` then mask |
| `0x1F` reads back as bank 31 | no masking by the bank count | `bank &= rom_bank_count - 1` |
| The ROM works until it needs a high bank, then jumps into garbage | the bank is computed once at load time and cached | read `gb->cart.rom_bank` on every access |
| `m08_mbc1_mode_ram` fails on the "disabled" assertion | you ignore `ram_enabled` | drop writes, return `0xFF` on read |
| Mode 1 does not change the RAM bank | `ram_bank` is not used in `cart_read`'s RAM branch | `ram_bank * 0x2000 + (addr - 0xA000)` |
| Mode 0 wrongly follows the secondary register into RAM | you use the 2-bit register for RAM in both modes | in mode 0 the RAM bank is 0 |
| RAM size is 0 after loading an 8 KiB-RAM cart | you allocated from the file size instead of the header | `cart_ram_size_bytes(ram_size_code)` |
| A 32 KiB ROM stops working | you applied MBC1 logic to `MBC_NONE` | `MBC_NONE` mirrors both halves |

---

## 5. Done when

- [ ] `m08_mbc1_bank0_quirk` and `m08_mbc1_mode_ram` are green
- [ ] `.\gb\build\gbemu.exe --rom <commercial game>.gb --frames 600` does not fault
- [ ] `-Test m0` shows no regression outside `m08`
- [ ] You can write MBC1's four command windows and the bank arithmetic from memory
- [ ] You can explain why ROM and RAM banking share one register
- [ ] Commit message like `feat(cart): MBC1 banking with the bank-0 quirk (L26)`

---

## 6. Optional, only if you have time

* `docs/03-memory-and-cartridge.md` §3.5 — the MBC table for every cartridge type, useful
  before you meet MBC2 (which has no separate RAM enable) in L28.
* Add a `--banks` debug flag that prints `rom_bank`, `ram_bank`, `mode`, `ram_enabled`
  whenever they change. When a game jumps into the wrong bank, this is the fastest way
  to see it.

Next: **[L27 — Save RAM](L27-save-ram.md)**
