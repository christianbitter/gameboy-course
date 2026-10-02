# L28 — MBC3, MBC5 and the RTC

**Time** — theory ~25 min | coding ~50 min | verify ~15 min
**You will end with** — the other two MBCs that matter, including the one piece of the
cartridge that measures *time*: MBC3's real-time clock, and the latch register that makes
it readable.
**Tests that must go green** — `m08_mbc3_rtc`, `m08_mbc5_9bit`
**Depends on** — L26 (the command-window model), L27 (`ram_enabled`, RAM banks). You will
edit `gb/src/cart.c`.

---

## 1. Read this — the whole theory for today

### The command-window model, applied twice more

L26's insight carries over unchanged: writes to `0x0000-0x7FFF` are **commands**, decoded
by address range, with the written byte as the argument.

**MBC3** (`0x0F-0x13`):

| Window | Command | Argument |
| --- | --- | --- |
| `0000-1FFF` | RAM/RTC enable | low nibble `0x0A` |
| `2000-3FFF` | ROM bank | **7 bits**; bank 0 is legal, no quirk |
| `4000-5FFF` | **selector** | `0x00-0x03` -> RAM bank; `0x08-0x0C` -> RTC register |
| `6000-7FFF` | latch clock data | write `0x00` then `0x01` |

**MBC5** (`0x19-0x1E`):

| Window | Command | Argument |
| --- | --- | --- |
| `0000-1FFF` | RAM enable | low nibble `0x0A` |
| `2000-2FFF` | ROM bank, **low 8 bits** | 0 is legal |
| `3000-3FFF` | ROM bank, **9th bit** | 0 or 1 |
| `4000-5FFF` | RAM bank | 4 bits (most cartridges wire only 2) |

MBC5 is the cleanest of the three, and the only one where the bank number is split across
two registers so that a game can set 512 banks without a read-modify-write dance. That
split is also why the ROM in `m08_mbc5_9bit` is **8 MiB**: with fewer than 257 banks the
9th bit is masked away and the test could not tell whether you read that register at all.

### MBC2, in one paragraph (optional)

`0x05-0x06` is MBC2: there is **no separate RAM enable**. The address's bit 8 decides what
a write to `0000-3FFF` means — bit 8 clear is the RAM enable, bit 8 set is the ROM bank.
It has 512 x 4 bits of internal RAM (so the upper nibble of each byte reads as `0xF0`) and
no external RAM at all. A handful of games use it. Implement it after the tests pass if
you want it; nothing in the ladder requires it.

### The RTC, and why a latch exists

MBC3 cartridges with a clock have five registers the game reads through `A000-BFFF` when
the selector is `0x08-0x0C`:

| Selector | Register | Range |
| --- | --- | --- |
| `0x08` | seconds | 0-59 |
| `0x09` | minutes | 0-59 |
| `0x0A` | hours | 0-23 |
| `0x0B` | days, low 8 bits | 0-255 |
| `0x0C` | days high bit, halt, day-carry | bit 0 = day bit 8, bit 6 = halt, bit 7 = carry |

The counters tick on their own. That creates a problem: a game reading seconds, then
minutes, can read across a carry — `0x3B` seconds then `0x00` minutes when the real time
is one minute later, and it constructs a timestamp that never existed. So the hardware
gives you **latching**: writing `0x00` then `0x01` to `0x6000-0x7FFF` copies all five live
counters into a snapshot, and **reads always return the snapshot**. The clock keeps
running; the snapshot does not move until the next latch.

That is the whole contract, and it is exactly what `m08_mbc3_rtc` checks: the test sets
the live registers, latches, then *changes the live seconds without latching* and requires
the read to still return the old value. An implementation that reads the live counters
passes the first half and fails that assertion — which is the point.

The RTC is driven by the host clock in real emulators, with the elapsed time written back
to the `.sav` so that closing and reopening the emulator does not stop the game's clock.
That is a convention, not a format (L27 notes the 44-byte footer some emulators use), and
no test here requires it. Implement the counters and the latch today; the wall-clock
plumbing is a stretch goal.

### Where the code goes

`cart_write` gains two more branches on `gb->cart.mbc`, and `cart_read`'s `A000-BFFF` path
becomes three-way: RAM bank, RTC register, or nothing when disabled:

```
if (!ram_enabled) return 0xFF;
if (mbc == MBC_3 && selector >= 0x08 && selector <= 0x0C)
    return rtc_latched[selector - 0x08];
return ram[ram_bank * 0x2000 + (addr - 0xA000)];
```

Note the order: the RTC check comes **before** the RAM indexing, because the selector
register is shared and means different things depending on its value.

> **What matters for the code you are about to write**
>
> * MBC3's `4000-5FFF` is a **selector**, not just a bank: `0x00-0x03` selects a RAM bank
>   and `0x08-0x0C` selects an RTC register. Store it as one field (a `u8` selector works
>   better than a `u8 ram_bank`) and interpret it on each access.
> * The latch is a **two-write sequence**: `0x00` arms it, `0x01` performs the copy. A
>   single write of `0x01` without a preceding `0x00` is not a latch.
> * Reads at `A000-BFFF` with an RTC selector must return **`rtc_latched[]`**, never
>   `rtc[]`. `m08_mbc3_rtc` is built to catch exactly that.
> * MBC5's ROM bank is `low8 | (bit9 << 8)`, masked by the cartridge's bank count. There
>   is **no** bank-0 substitution: `m08_mbc5_9bit` asserts bank 0 is visible.
> * MBC5's RAM enable is the same `0x0A` low-nibble check, and its RAM bank register is a
>   plain 4-bit value at `4000-5FFF`.
> * MBC3's ROM bank register is 7 bits (mask `0x7F`), so mask it before the bank-count
>   mask, and remember bank 0 is legal for MBC3 — the MBC1 quirk does **not** apply.
> * `m08_mbc3_rtc` sets `gb->cart.rtc[]` directly, the same way the PPU tests set
>   `gb->ppu.*`. Your field names must match those in `include/gb/cart.h`:
>   `rtc[5]`, `rtc_latched[5]`, `rtc_latch_armed`.

---

## 2. Your task — 50 min

Work in `gb/src/cart.c`.

1. **MBC3 ROM and RAM banking** (15 min). `0x2000-0x3FFF` -> 7-bit bank with no quirk;
   `0x4000-0x5FFF` -> store the selector; `A000-BFFF` -> RAM bank when the selector is
   `0x00-0x03`.
2. **The RTC registers and the latch** (20 min). `rtc_latch_armed` for the two-write
   sequence, then `memcpy(rtc_latched, rtc, 5)`. Reads with a selector of `0x08-0x0C`
   return `rtc_latched[selector - 8]`.
3. **MBC5** (15 min). Two registers for the 9-bit bank, no quirk, bank 0 legal, and the
   4-bit RAM bank register. Then run both tests.
4. **Run the tests** (5 min):
   ```
   .\gb\build.cmd -Test m08
   ```
   All five green.

---

## 3. Prove it — 15 min

```
.\gb\build.cmd -Test m08
```
```
   5 passed,    0 failed,    0 skipped
ALL GREEN
```
Then check the cartridges you actually have. `--info` on a Pokémon-era cartridge should
say MBC3 with a timer; on a late cartridge, MBC5. Both should boot:

```
.\gb\build\gbemu.exe --rom <an MBC3 game with a clock>.gb --info
.\gb\build\gbemu.exe --rom <an MBC5 game>.gb --frames 600 --dump-frame build\mbc5.bmp
```

For an MBC3 game with a time-of-day system, the visible check is that the clock advances:
run it, note the in-game time, run it again after a minute of real time, and confirm the
game shows a later time. If time is frozen, your live `rtc[]` counters are not being
ticked; if time jumps wildly, the latch is returning live values instead of the snapshot.

```
.\gb\build.cmd -Test m0
```
Regression: nothing outside `m08` should change.

---

## 4. If it fails

| Symptom | Cause | Find it |
| --- | --- | --- |
| `m08_mbc3_rtc` reads the live value | you return `rtc[]` instead of `rtc_latched[]` | the read must come from the snapshot |
| The latch never happens | one write is treated as a latch | `0x00` arms, `0x01` copies |
| RTC register 8 reads RAM instead | the selector range check is wrong | `0x08-0x0C` is RTC, `0x00-0x03` is RAM |
| Selecting 0x04 reads garbage as RAM | you accept any selector `<= 3` incorrectly, or index RAM with the selector | validate the range, or index with `selector` only for `0x00-0x03` |
| `m08_mbc5_9bit` never sees the 9th bit | you wrote the low byte to both registers, or ignore `3000-3FFF` | two separate fields |
| MBC5 shows bank 1 when you write 0 | you reused MBC1's bank-0 quirk | MBC5 has no quirk |
| MBC3 mask gives bank 0x45 for a 7-bit value | you masked with `0x1F` | MBC3 is 7 bits |
| ROM runs until it needs bank 0x100 then faults | the 9th bit is not included in the bank index | `(low8 \| (bit9 << 8))` before masking |
| The tests pass but a real MBC5 game fails | its RAM bank register is 4 bits and you mask to 2 | mask by `ram_bank_count`, not by 2 |

---

## 5. Done when

- [ ] All five `m08_*` tests are green
- [ ] `--info` correctly identifies an MBC3 and an MBC5 cartridge from your own collection
- [ ] An MBC3 game's clock advances between two runs
- [ ] `-Test m0` shows no regression
- [ ] You can state the four command windows for MBC3 and MBC5 from memory
- [ ] You can explain why the RTC needs a latch, in terms of reads that would otherwise
      straddle a carry
- [ ] Commit message like `feat(cart): MBC3, MBC5 and the RTC latch (L28)`

---

## 6. Optional, only if you have time

* `docs/03-memory-and-cartridge.md` §3.5 — the full cartridge-type table, including MBC2
  and the MMM01/HuC oddities you will almost certainly never need.
* Implement the RTC ticking from the host clock plus a `.sav` footer so the in-game clock
  survives a restart. It is the last piece of "a real cartridge behaves like a real
  cartridge", and it is a nice warm-up for L32's save states, which have a very similar
  problem.

Next: **[L29 — Access-level timing](L29-access-level-timing.md)**
