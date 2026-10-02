# L15 — Interrupts and HALT

**Time** — theory ~25 min | coding ~50 min | verify ~15 min
**You will end with** — a machine that reacts: five interrupt sources, the one-instruction
`EI` delay, and `HALT`. AHA: *a peripheral can now take control of my CPU, which is the
moment it stops being a calculator.*
**Tests that must go green** — `m05_interrupt_dispatch`, `m05_ime_ei_delay`,
`m05_halt_wake`, `m05_halt_bug`
**Depends on** — L13 (the push helper). You will edit `gb/src/cpu.c` and `gb/src/opcodes.c`.

---

## 1. Read this — the whole theory for today

### Three registers, one of them not memory

| Register | Address | Meaning |
| --- | --- | --- |
| `IF` | `FF0F` | **requested** — bits 0-4, set by peripherals |
| `IE` | `FFFF` | **enabled** — bits 0-4, set by the program |
| `IME` | not mapped | master switch; only `EI`, `DI`, `RETI` and the dispatcher touch it |

In this codebase `IF` is `GB_IF(gb)` (`bus.io[0x0F]`) and `IE` is `GB_IE(gb)`
(`bus.ie`). Peripherals raise an interrupt with the provided
`bus_request_interrupt(gb, bits)` — the timer calls it in L16, the PPU in L18, serial in
L17, the joypad in L19. They never touch `IE`.

### The five sources

| bit | source | vector | raised when |
| --- | --- | --- | --- |
| 0 | VBlank | `0x0040` | the PPU enters mode 1 |
| 1 | LCD STAT | `0x0048` | an enabled STAT condition goes true |
| 2 | Timer | `0x0050` | `TIMA` overflows |
| 3 | Serial | `0x0058` | a transfer completes |
| 4 | Joypad | `0x0060` | a selected key goes down |

### Dispatch: before the fetch, lowest bit wins

```
pending = IE & IF & 0x1F
if (IME && pending) {
    bit = the LOWEST set bit of pending      /* VBlank beats the timer, always */
    IME = 0
    clear that bit in IF
    push PC                                   /* high byte to SP-1, low to SP-2 */
    PC = 0x40 + bit * 8
    halted = false
    return 20                                  /* 5 M-cycles */
}
```

Three consequences worth internalising:

* **Priority is by bit number, not by arrival time.** If VBlank and the timer both
  want attention, VBlank goes first because bit 0 < bit 2.
* **The dispatch is checked before the next instruction is fetched**, so a pending
  interrupt preempts that instruction rather than running after it.
* **`IF` is cleared by the dispatcher, not by the peripheral.** The peripheral's job is
  only to raise the bit; the request stays raised until it is served.

### The order of business in `cpu_step`

This ordering is not a style choice — `m05_ime_ei_delay` fails if you get it wrong:

```
1. if (fatal) return 0;
2. if (IME && pending)          -> dispatch, return 20      /* uses the CURRENT IME */
3. if (ime_pending) { IME = 1; ime_pending = 0; }           /* promote AFTER (2)   */
4. if (halted) return 4;                                    /* no fetch            */
5. fetch, execute, return the opcode's cycles
```

Why step 3 must come *after* step 2: `EI` takes effect only after the instruction
following it has executed. If you promoted `IME` before testing for a pending
interrupt, the interrupt would preempt the instruction after `EI` — which is exactly
what the hardware does not do, and exactly what the test checks.

### `EI`, `DI`, `RETI`, `HALT`

| Opcode | Instruction | Effect |
| --- | --- | --- |
| `0xF3` | `DI` | `IME = 0` **immediately**, and cancel any pending `EI` |
| `0xFB` | `EI` | set `ime_pending`; `IME` becomes 1 after the next instruction retires |
| `0xD9` | `RETI` | pop `PC` (L13) **and** set `IME = 1` immediately — no delay |
| `0x76` | `HALT` | stop fetching until `IE & IF & 0x1F` is non-zero |

`HALT` has one subtlety that a test pins down. The handler must be:

```
halted = true;
if (IE & IF & 0x1F) {
    halted = false;        /* nothing to wait for: do not halt */
    /* the "halt bug" double-fetch belongs here too — that is L31 */
}
return 4;
```

With `IME = 1` and something pending, `HALT` does not halt and the *next* `cpu_step`
dispatches (step 2 above does it for you). With `IME = 0` and something pending, the CPU
must also not stay halted — `m05_halt_bug` asserts exactly that, and only that. The
precise "the byte after `HALT` is fetched twice" behaviour is deferred to L31 where
blargg's `halt_bug.gb` can verify it.

> **What matters for the code you are about to write**
>
> * Put the dispatch at the **top** of `cpu_step`, before the fetch, returning **20**.
> * Use the **lowest set bit** of `pending`: `while (!(pending & (1u << bit))) bit++;`
> * Clear the served bit in `IF`, clear `IME`, clear `halted`, push `PC` with L13's
>   `push16` helper, then `PC = GB_INT_VECTOR(bit)`.
> * Order: dispatch (2) **then** promote `ime_pending` (3). Reversing them fails
>   `m05_ime_ei_delay`.
> * `DI` clears both `IME` and `ime_pending`. `RETI` sets `IME` immediately, no delay.
> * `HALT` sets `halted` **only** if `IE & IF & 0x1F` is zero. When an interrupt is
>   dispatched, clear `halted`.
> * `halted` short-circuits the fetch: a halted `cpu_step` returns 4 and does not
>   advance `PC`. The test asserts `PC` stays put for a whole step.
> * `bus_request_interrupt` already exists — do not write your own IF manipulation in
>   the peripherals; `IF`'s bits 5-7 read as 1 and the provided helper keeps that
>   invariant.

---

## 2. Your task — 50 min

Work in `gb/src/cpu.c` (`cpu_step`) and `gb/src/opcodes.c` (`DI`/`EI`/`RETI`/`HALT`).

1. **`cpu_step`'s order of business** (20 min). Implement steps 1-5 exactly as listed.
   Keep the existing fetch/execute tail, and add the interrupt dispatch in front of it.
2. **`DI`/`EI`** (10 min). `0xF3` clears `ime` and `ime_pending`; `0xFB` sets
   `ime_pending`. Four T-cycles each.
3. **`RETI`** (5 min). Reuse L13's pop; then set `ime = true` and clear `ime_pending`.
4. **`HALT`** (10 min). `0x76`, four T-cycles, and the conditional from §1. Keep your
   dispatcher's `GB_UNIMPLEMENTED("HALT")` comment as the thing you are replacing.
5. **Run the tests** (5 min):
   ```
   .\gb\build.cmd -Test m05
   ```
   Four green (this lesson), three red (`m05_timer_*` — L16), one red
   (`m05_serial_emits_byte` — L17).

---

## 3. Prove it — 15 min

```
.\gb\build.cmd -Test m05
```
```
[       OK ] m05_halt_bug
[       OK ] m05_halt_wake
[       OK ] m05_ime_ei_delay
[       OK ] m05_interrupt_dispatch
[  FAILED  ] m05_timer_div_rate        <- L16
...
   4 passed,    4 failed,    0 skipped
```
Then prove it end to end: write a scratch program with a VBlank handler installed at
`0x0040`:

```
0000: 00 00 00   (vectors live at 0x40, not here)
0040: 3C         INC A            <- VBlank handler
0041: D9         RETI
0100: FB         EI
0101: 00 00 00   NOP NOP NOP      <- the loop
```

Force `IF |= VBLANK` and `IE = VBLANK`, run a few steps, and watch `A` increment each
time the request is raised. That single trace is the whole interrupt mechanism in one
screen, and it is also the first time you have seen a *handler* run.

```
.\gb\build.cmd -Test m0
```
Regression: `m00`-`m04` fully green, `m05` with the four expected reds.

---

## 4. If it fails

| Symptom | Cause | Find it |
| --- | --- | --- |
| `m05_ime_ei_delay` dispatches one step too early | you promoted `ime_pending` before the dispatch test | dispatch (2) then promote (3) |
| `m05_ime_ei_delay`: `IME` is still false after the NOP | you promoted nothing, or promoted at the start of the *following* step | promote at the end of the step whose start saw `ime_pending` |
| The interrupt is dispatched but `IF` still has the bit set | you cleared `IME` but not the served bit | clear `pending`'s lowest bit in `IF` |
| The pushed `PC` is off by two | you pushed after assigning `PC`, or used a computed return address | push the *current* `PC`, then assign |
| The timer interrupt is taken before VBlank when both are pending | you scanned for the highest set bit | lowest bit wins |
| `m05_halt_wake`: the CPU stays halted forever | the dispatch path does not clear `halted` | clear it when you dispatch |
| `m05_halt_bug` fails with `halted == 1` | you set `halted` unconditionally | only halt when nothing is pending |
| After `HALT`, `PC` advances anyway | you fetched before checking `halted` | `halted` returns 4 immediately |
| `RETI` behaves like `RET` and the next interrupt never fires | you forgot `ime = true` | `RETI` enables immediately |

---

## 5. Done when

- [ ] The four `m05_*` interrupt/HALT tests are green
- [ ] Your scratch VBlank-handler program increments `A` once per raised request
- [ ] `-Test m0` shows no regression
- [ ] You can state the five vectors and their bits without looking
- [ ] You can explain why the `EI` delay exists in hardware terms, not just as a rule
- [ ] You can explain why the peripheral only raises `IF` and never touches `IE`
- [ ] Commit message like `feat(cpu): interrupt dispatch, EI delay, HALT (L15)`

---

## 6. Optional, only if you have time

* `docs/02-cpu.md` §2.6 — the same mechanism plus the `RETI`-versus-`EI`-`RET` idiom and
  the interrupt-timing corner cases that L29 will care about.
* Make `--break-op` in `main.c` also stop on a *vector* being taken (e.g.
  `--break-vector 0`). Ten minutes, and it turns "why did my handler not run" into a
  single command.

Next: **[L16 — The timer](L16-the-timer.md)**
