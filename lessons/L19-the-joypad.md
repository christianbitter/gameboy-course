# L19 — The joypad

**Time** — theory ~20 min | coding ~50 min | verify ~20 min
**You will end with** — input: a 2x4 key matrix on `FF00`, with an edge-triggered
interrupt. That is the last peripheral phase 1 needs, and the last thing between you and
running a ROM.
**Tests that must go green** — `m06_joypad_matrix`
**Depends on** — L03 (I/O routing), L15 (`bus_request_interrupt`), L17 (`LDH` — games
read the joypad with `LDH A,(0x00)`). You will edit `gb/src/joypad.c` and `gb/src/bus.c`.

---

## 1. Read this — the whole theory for today

### One register, two 2x4 matrices

`FF00` (`P1`/`JOYP`) is not a single value: it is two four-key matrices, and bits 4 and 5
of whatever you *write* select which one the read reports.

```
write bit 4 = 0  ->  the DIRECTION keys are selected
write bit 5 = 0  ->  the BUTTON  keys are selected
```

| Read bit | With `P14 = 0` (directions) | With `P15 = 0` (buttons) |
| --- | --- | --- |
| 0 | Right | A |
| 1 | Left | B |
| 2 | Up | Select |
| 3 | Start (the direction group's bit 3) | Down |

Careful with that last column: the button group's bit 3 is **Start**, and the direction
group's bit 3 is **Down**. The pairs line up by bit position, not by name.

The other rules:

* **`0` means pressed**, both for the selection bits and for the key bits. Selecting a
  group is `0`; a key that is held reads `0`.
* A group that is **not** selected reads as all 1s (nothing pressed).
* **Bits 6 and 7 always read 1.**
* Writing `0x30` deselects both groups; writing `0x00` selects both; `0x10` selects
  directions only; `0x20` selects buttons only.

So the whole read is:

```
out  = 0xC0 | (select & 0x30)                 /* bits 6-7 set, selection echoed */
keys = 0x0F
if (select & 0x10) == 0:   clear bits for the held direction keys
if (select & 0x20) == 0:   clear bits for the held button keys
return out | keys
```

### The interrupt is a falling edge, not a level

`IF` bit 4 is requested when **a selected key bit goes from 1 to 0** — that is, at the
moment a key is pressed (or at the moment a group is selected while a key is already
held). Holding a key does not keep requesting; releasing it does not request either.

That is why `joypad_t` has a `prev_read` field: the edge is a property of the previous
read versus the current one.

```
if ((prev_read & ~now) & 0x0F) != 0:   bus_request_interrupt(gb, GB_INT_JOYPAD);
prev_read = now;
```

`m06_joypad_matrix` checks both halves of that: pressing raises `IF` bit 4, and reading
again while the key is still held does not.

### Who owns what

| Piece | Where | Who |
| --- | --- | --- |
| Which group is selected | `joypad_t.select` | `bus_write(FF00)` stores `value & 0x30` |
| Which keys are held | `joypad_t.pressed`, one bit per `GB_BTN_*` | the host, via the provided `joypad_set(gb, button, down)` |
| The register value and the edge | computed on every read | `joypad_read(gb)`, which you write |

`bus_read(FF00)` must call `joypad_read(gb)` — not return a stored byte — because the
read is what performs the edge detection. That is the third instance of "I/O reads have
side effects" (`DIV`, `STAT`, and now `P1`).

`gb/include/gb/joypad.h` already defines the button enum in the right order:

| `GB_BTN_*` | A | B | SELECT | START | RIGHT | LEFT | UP | DOWN |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| bit in `pressed` | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 |

Note that `pressed`'s bit order (A, B, Select, Start, Right, Left, Up, Down) is **not**
the register's bit order, which is why the read function has to translate. The first
four map to the button group in order; the last four map to the direction group in
order.

> **What matters for the code you are about to write**
>
> * `bus_write(FF00, v)` stores `v & 0x30` in `joypad.select`. Do not store the whole
>   byte: the key bits are not writable.
> * `bus_read(FF00)` returns `joypad_read(gb)`, and `joypad_read` is where the edge
>   detection happens. Reading the register twice must be able to produce two different
>   values.
> * The group tests are `!(select & 0x10)` for directions and `!(select & 0x20)` for
>   buttons: the selection bits are **active low**.
> * Map the direction group to bits 0-3 as Right, Left, Up, Down and the button group
>   as A, B, Select, Start. `m06_joypad_matrix` fails with a specific key name if you
>   swap two of them.
> * The edge condition is `(prev_read & ~now) & 0x0F`. One expression, and the test
>   checks both that it fires and that it does not re-fire while held.
> * Always set bits 6-7 in the returned value. The test asserts `read & 0xC0 == 0xC0`.
> * `joypad_reset` is provided (a `memset`). `select = 0` means both groups selected,
>   which with nothing held produces the power-on value `0xCF` — the same value the
>   post-boot table lists for `FF00` in L18.

---

## 2. Your task — 50 min

Work in `gb/src/joypad.c`, plus the `FF00` cases in `gb/src/bus.c`.

1. **Routing** (10 min). `bus_write(FF00, v)` -> `gb->joypad.select = v & 0x30;`.
   `bus_read(FF00)` -> `joypad_read(gb)`.
2. **The matrix** (25 min). Implement `joypad_read` from the rules above. Write the key
   translation as two small blocks rather than one clever expression — you will read
   this function again when you add a host front end in L34.
3. **The edge** (10 min). Compare against `prev_read` before updating it, and raise
   `GB_INT_JOYPAD` via `bus_request_interrupt`.
4. **Run the tests** (5 min):
   ```
   .\gb\build.cmd -Test m06
   ```
   `m06_joypad_matrix` green, all four non-ROM `m06_*` tests green.

---

## 3. Prove it — 20 min

```
.\gb\build.cmd -Test m06
```
```
[       OK ] m06_joypad_matrix
[  FAILED  ] m06_run_60_frames         <- needs a real ROM's worth of correctness
```
Then play with it from the test side, which is the only "input device" you have until
L34. Write a scratch test that:

1. selects the button group (`bus_write(0xFF00, 0x20)`),
2. calls `joypad_set(gb, GB_BTN_A, true)`,
3. reads `FF00` and confirms bit 0 is 0,
4. sets `IF` to 0, releases A, then presses Right with the direction group selected and
   confirms `IF` bit 4 is set.

Step 4 is the interesting one: it proves the interrupt is a *transition*, not a level.
Then run a program that polls the joypad in a loop (a scratch `LDH A,(0x00)` /
`AND 0x01` / `JR NZ,-4`) and watch `PC` stay in the loop until you call `joypad_set`.
An input-polling program that reacts to your own function call is a good moment.

```
.\gb\build.cmd -Test m0
```
Regression: `m00`-`m05` fully green, `m06` with one red.

---

## 4. If it fails

| Symptom | Cause | Find it |
| --- | --- | --- |
| Nothing reads as pressed | you tested `(select & 0x10)` instead of `!(select & 0x10)` | selection is active low |
| Directions work, buttons do not (or the reverse) | `P14`/`P15` swapped | `0x10` is directions, `0x20` is buttons |
| A key maps to the wrong register bit | group/bit mix-up, e.g. Start vs Down at bit 3 | the two tables in §1 |
| `read & 0xC0` is `0x00` | you returned only the key bits | bits 6-7 are always 1 |
| The interrupt fires once and never again | you never updated `prev_read` | update it on every read |
| The interrupt fires on every read while held | you used `(now & ~prev)` or a level test | it is `(prev & ~now)` |
| The interrupt fires when a key is *released* | direction of the edge is inverted | falling edge only |
| `-Test m0` regresses in `m02` | you changed `op_execute`'s default case while adding `LDH` earlier | only `joypad.c` and `bus.c` should change today |

---

## 5. Done when

- [ ] `m06_joypad_matrix` is green
- [ ] Your scratch test shows the interrupt firing on press and not on hold
- [ ] `-Test m0` shows no regression
- [ ] You can write the `FF00` bit table from memory, directions and buttons
- [ ] You can explain why `bus_read(FF00)` must compute rather than return a stored byte
- [ ] Commit message like `feat(joypad): the FF00 matrix and its edge-triggered interrupt (L19)`

---

## 6. Optional, only if you have time

* `docs/04-ppu-and-peripherals.md` §4.4 — the joypad section, including the
  `P1 = 0xCF` post-boot value you set in L18 and why it is consistent with `select = 0`.
* Map the arrow keys and `Z`/`X` in `main.c` now via a `--keys` stub you fill in later.
  It costs nothing today and means L34 is only a windowing exercise.

Next: **[L20 — A real ROM runs](L20-a-real-rom-runs.md)**
