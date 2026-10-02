# L16 — The timer

**Time** — theory ~25 min | coding ~50 min | verify ~15 min
**You will end with** — `DIV`, `TIMA`, `TMA`, `TAC`, a free-running divider, and an
interrupt source you can actually observe ticking. AHA: *a peripheral now generates
events on its own.*
**Tests that must go green** — `m05_timer_div_rate`, `m05_timer_tima_overflow`,
`m05_timer_tma_reload`
**Depends on** — L15 (`bus_request_interrupt`, `GB_IF`). You will edit `gb/src/timer.c`
and `gb/src/bus.c`.

---

## 1. Read this — the whole theory for today

### One counter, four registers

The timer is a single 16-bit counter clocked by the **master clock**: it advances by 1
every T-cycle. Everything else is derived from it.

| Register | Address | What it actually is |
| --- | --- | --- |
| `DIV` | `FF04` | the **high byte** of that counter |
| `TIMA` | `FF05` | a separate 8-bit counter that `TAC` decides when to increment |
| `TMA` | `FF06` | the value `TIMA` reloads with when it overflows |
| `TAC` | `FF07` | bit 2 = enable, bits 1-0 = which divider bit clocks `TIMA` |

Because `DIV` is the high byte, `DIV` increments every 256 T-cycles — 4194304 / 256 =
**16384 Hz**. That is why `DIV` is a useful rough clock even though nothing in your code
"counts" it. **Writing `DIV` resets the whole counter to 0**, which is a side effect,
not a store.

In this codebase the counter is `gb->timer.div_counter` and `DIV` is
`(div_counter >> 8) & 0xFF` — computed on read, never stored separately. If you keep a
`div` byte as well, the two will drift apart and `m05_timer_div_rate` will catch it.

### `TAC` picks a bit, and the bit's falling edge is the tick

| `TAC` bits 1-0 | bit of the divider | TIMA period | frequency |
| --- | --- | --- | --- |
| `00` | bit 9 | 1024 T | 4096 Hz |
| `01` | bit 3 | 16 T | 262144 Hz |
| `10` | bit 5 | 64 T | 65536 Hz |
| `11` | bit 7 | 256 T | 16384 Hz |

`TIMA` increments when the selected bit goes **1 -> 0**. That is why the period is
`2^(n+1)`: bit `n` is high for `2^n` ticks and low for `2^n` ticks.

Concretely, with the counter starting at 0 after a `DIV` write:

* rate `01` (bit 3, value 8): the bit is high while the counter is 8..15 and falls at
  counter = 16 -> **one `TIMA` tick after 16 T-cycles**.
* rate `00` (bit 9, value 512): falls at counter = 1024 -> one tick after **1024**
  T-cycles.

The practical implementation is a loop with one remembered bit:

```
for each T-cycle:
    div_counter++
    bool now = (div_counter >> bit_index) & 1
    if (bit_was_set && !now) { tima++; if (tima overflowed) { tima = tma; raise IF bit 2 } }
    bit_was_set = now
```

You cannot bolt this on as "TIMA increments every N ticks" without a starting phase: the
tests reset `DIV` first so the phase is known, and a naive period counter that starts
mid-cycle will be off by up to a full period. The remembered-bit form is both simpler and
correct.

### Overflow

When `TIMA` goes `0xFF -> 0x00`:

```
TIMA = TMA
bus_request_interrupt(gb, GB_INT_TIMER)      /* IF bit 2 */
```

That is it for today. Hardware actually delays the reload by 4 T-cycles, during which
`TIMA` reads `0x00` and keeps counting; that quirk is **L30**. `m05_timer_tima_overflow`
and `m05_timer_tma_reload` test the clean model and nothing more — which is why the
second one only needs "and then it keeps counting from `TMA`": tick 16 gives `0x42`,
another 16 gives `0x43`.

### The bus routing, and the disabling case

| Address | Read | Write |
| --- | --- | --- |
| `FF04` | `(div_counter >> 8) & 0xFF` | `div_counter = 0` (and reset the phase) |
| `FF05` | `timer.tima` | `timer.tima = value` |
| `FF06` | `timer.tma` | `timer.tma = value` |
| `FF07` | `timer.tac \| 0xF8` | `timer.tac = value & 0x07` |

Only bits 0-2 of `TAC` exist; the rest read as 1. Writing `TAC` with bit 2 clear
**stops `TIMA` but not `DIV`** — `m05_timer_div_rate`'s last block checks exactly that:
`TIMA` frozen for 4096 ticks while `DIV` moves.

`timer_tick(gb, tcycles)` is called by `bus_tick()`, which is L18's job. The tests call
it directly with an exact cycle count, so you can finish the timer before there is a
clock feeding it.

> **What matters for the code you are about to write**
>
> * `DIV` is **derived**: `(div_counter >> 8) & 0xFF`. Never store a `div` field.
> * The counter advances **1 per T-cycle**; `div_counter` is `u16`, so it wraps every
>   65536 T-cycles on its own — that is correct hardware behaviour, not a bug.
> * Remember the selected bit's previous value and increment `TIMA` on a **1 -> 0**
>   transition. Recompute the bit index from `TAC` each tick (or when `TAC` changes);
>   `tac = 0` disables the increment entirely.
> * On overflow: `tima = tma` and `bus_request_interrupt(gb, GB_INT_TIMER)`.
> * Writing `FF04` must zero the counter **and** the remembered bit, or the next test's
>   phase is wrong.
> * Writing `FF05`/`FF06`/`FF07` must **not** disturb `div_counter`.
> * Store only `value & 0x07` in `tac`, and return `tac | 0xF8` on read.
> * `timer_reset` is provided as a `memset`; it is enough, because `div_counter = 0`
>   is a legitimate power-on state and the tests always write `DIV` first.

---

## 2. Your task — 50 min

Work in `gb/src/timer.c`, plus the `FF04-FF07` cases in `gb/src/bus.c`.

1. **Bus routing for the timer registers** (15 min). Add the four cases to your I/O
   `switch`, using the table above. This is the first peripheral you are wiring to the
   ownership table from L03, so get the shape right — every later peripheral follows it.
2. **`timer_tick`** (25 min). Tick one T-cycle at a time. Track the selected bit's
   previous value in a local or in the struct — if you put it in the struct, add it to
   `timer_t` (it is your struct to extend) so it survives across calls. `m05_timer_div_rate`
   calls `timer_tick` twice in a row with the same `TAC`, so a local will not do.
3. **Overflow** (5 min). Reload from `TMA` and raise `GB_INT_TIMER` via
   `bus_request_interrupt`.
4. **Run the tests** (5 min):
   ```
   .\gb\build.cmd -Test m05
   ```
   Seven green, one red (`m05_serial_emits_byte` — L17).

---

## 3. Prove it — 15 min

```
.\gb\build.cmd -Test m05
```
```
[       OK ] m05_timer_div_rate
[       OK ] m05_timer_tima_overflow
[       OK ] m05_timer_tma_reload
[  FAILED  ] m05_serial_emits_byte      <- L17
...
   7 passed,    1 failed,    0 skipped
```
Now watch it work. Write a scratch program that sets `TAC = 0x05`, `TMA = 0x00`,
`TIMA = 0xFF`, enables the timer interrupt, and then just spins:

```
0100: 3E 05      LD A,0x05
0102: E0 07      LDH (0x07),A     <- TAC  (needs LDH, or use EA 07 FF)
...
```

If `LDH` is not implemented yet, do it from the test side instead: enable the timer,
run 200 T-cycles of `timer_tick`, and print `bus_read(FF04)` and a counter of how many
times `IF` bit 2 was set. Seeing `DIV` climb at exactly 1 per 256 ticks and the timer
request appear every 16 is the moment the timer stops being a table and becomes a clock.

```
.\gb\build.cmd -Test m0
```
Regression: `m00`-`m04` green, `m05` with one expected red.

---

## 4. If it fails

| Symptom | Cause | Find it |
| --- | --- | --- |
| `m05_timer_div_rate`: `DIV` reads 1 after 512 ticks | you advance `div_counter` once per 2 T-cycles, or stored a `div` byte | 1 per T-cycle, `DIV` is `>> 8` |
| `DIV` reads 2 when 1 is expected | you derived `DIV` from a byte you increment yourself | derive it; never store it |
| `TIMA` ticks after 512 instead of 1024 for rate `00` | you increment on the **rising** edge, or used a period of `2^n` | falling edge, period `2^(n+1)` |
| `TIMA` never ticks | `TAC` bit 2 is ignored (you enable on write, not on tick) | check the enable bit inside the tick loop |
| The first tick happens too early | you did not reset the remembered bit when `DIV` was written | reset it with the counter |
| `m05_timer_tma_reload` stops at `0x42` | you disabled the timer on overflow | it keeps running from `TMA` |
| Overflow sets the flag but `TIMA` stays `0x00` | you raised `IF` but forgot `tima = tma` | both, together |
| `TAC` reads `0x04` instead of `0xFC` | you store the raw byte | store `& 0x07`, read `\| 0xF8` |
| `FF04` reads `0x00` after a write of `0xAB` | you stored the written value | writing `DIV` resets the counter; reads derive it |

---

## 5. Done when

- [ ] The three timer tests are green, leaving only the serial test red in `m05`
- [ ] Your scratch run shows `DIV` at 16384 Hz and the timer request at the selected rate
- [ ] `-Test m0` shows no regression
- [ ] You can state the four `TAC` rates and their divider bits without looking
- [ ] You can explain why "increment `TIMA` every N cycles" is wrong on the first tick
- [ ] Commit message like `feat(timer): DIV/TIMA/TMA/TAC with falling-edge ticking (L16)`

---

## 6. Optional, only if you have time

* `docs/03-memory-and-cartridge.md` §3.2 — the "I/O reads have side effects" rule, of
  which `DIV` is the cleanest example.
* Peek ahead at `docs/04-ppu-and-peripherals.md` §4.5 (serial) and read the whole of L17
  before you start it — it is the shortest lesson in the ladder and the one that unlocks
  the test ROMs.

Next: **[L17 — The last opcodes, and serial](L17-the-last-opcodes-and-serial.md)**
