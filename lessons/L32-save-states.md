# L32 — Save states

**Time** — theory ~20 min | coding ~55 min | verify ~15 min
**You will end with** — a snapshot of the entire machine, saved to a file and restored
exactly. AHA: *"everything that is not in the snapshot is a bug I will find later."*
**Tests that must go green** — `m11_savestate_roundtrip`
**Depends on** — the whole machine so far. You will implement `gb/src/savestate.c` (the file
and its header already exist with the contract written out).

---

## 1. Read this — the whole theory for today

### What a save state is

A save state is a snapshot of **everything that makes the machine what it is**, such that
loading it resumes exactly where saving left off. That definition sounds obvious and is
brutally precise: anything mutable that you forget is a bug that appears minutes later as
"the game is subtly wrong after load", which is one of the hardest classes of bug to
attribute.

The machine is already organised as one struct — that was L01's decision — so the working
set is enumerable rather than guessable:

| Component | What must be captured |
| --- | --- |
| `cpu` | every register, `pc`, `sp`, `ime`, `ime_pending`, `halted`, `halt_bug`, and the per-instruction cycle counter |
| `bus` | `vram`, `wram`, `oam`, `hram`, `io`, `ie`, and the DMA transfer state |
| `cart` | `ram` — plus the MBC latches (`rom_bank`, `ram_bank`, `mode`, `ram_enabled`, the MBC3 selector and RTC) |
| `timer` | `div_counter`, `tima`, `tma`, `tac`, the reload-delay state |
| `ppu` | every register, `dot`, `mode`, `window_line`, `stat_line`, the **framebuffer** |
| `joypad` | `select`, `pressed`, `prev_read` |
| `serial` | `sb`, `sc`, the transfer state |
| machine | `total_ticks`, `frame_count` |

### What must NOT be captured

Three things, for three different reasons:

* **The ROM.** It comes from the `.gb` file and is up to 8 MiB. Serialising it makes every
  save state huge and every load slow, for no benefit.
* **Host configuration**: `serial_out`, `headless`, the file paths, the fatal handler.
  These belong to the process, not to the machine. Restoring a stale `FILE *` from a state
  file is a good way to crash.
* **The tracer ring.** It is debug scaffolding. Restoring it would make a loaded state
  reproduce the previous session's instruction history, which is confusing rather than
  useful.

The rule that covers all three: *a save state contains the machine, not the emulator.*

### The framebuffer counts

It is tempting to skip the framebuffer because it "gets redrawn next frame". It does — but
only the *visible* lines, and only after the PPU reaches them. Load a state and you will see
one stale frame, which is exactly the kind of one-frame glitch that makes you distrust your
own emulator. It is 23,040 bytes; include it. `m11_savestate_roundtrip` checks it for this
reason.

### The format: be boring

```
offset  size  contents
0       4     magic, e.g. "GBST"
4       2     version (bump it whenever the field list changes)
6       4     payload length
10      ...   every field, in a fixed order
```

Compression, forward compatibility and variable-length encoding are all premature. What you
want from a save-state format is *debuggability*: when it fails, you want to be able to hex
dump it and see a field. A version tag gives you the one compatibility feature that matters
— refusing a state written by a different build instead of misinterpreting its bytes.

### The cursor pattern

Do not write a chain of `fwrite` calls with a running offset; write a tiny cursor:

```c
typedef struct { u8 *p, *end; bool overflow; u32 written; } wcursor_t;

static void w8 (wcursor_t *c, u8 v)  { if (c->p + 1 <= c->end) { *c->p = v; c->p++; } else c->overflow = true; c->written++; }
static void w16(wcursor_t *c, u16 v) { w8(c, (u8)v); w8(c, (u8)(v >> 8)); }
static void w32(wcursor_t *c, u32 v) { w16(c, (u16)v); w16(c, (u16)(v >> 16)); }
static void wmem(wcursor_t *c, const void *src, size_t n) { ... }
```

Every size check is then in one place, and the "insufficient buffer" case is a single flag
check at the end — which is what `gb_save` must return 0 on. Reading mirrors it with an
`overrun` flag, and the reader returns false rather than reading past the end.

### Parse into a temporary, then commit

`gb_load` must return **false and change nothing** for a buffer it did not produce. That is
easy if you validate first:

```
1. check len >= header size, magic, version, and that payload length fits in len
2. check that len is at least the size this version expects
3. decode into a temporary, or decode into the machine and bail out early while
   nothing has been committed
```

The test feeds `gb_load` four bytes of `DE AD BE EF` and requires the machine to be
unchanged afterwards, so "partially applied garbage" fails.

### The APU will arrive later

L35/L36 add channel state to the machine. On that day, `m11_savestate_roundtrip` still
passes — because it does not check the APU — and your save states silently lose the audio
channels. That is precisely the failure mode this lesson is about. Write the field list in
one place with a comment for each component, and add a note to `NOTES.md`: *"when adding
machine state, add it to savestate.c."*

> **What matters for the code you are about to write**
>
> * `gb_save` returns the number of bytes written, or **0** if `cap` is too small — and it
>   must not run past `cap` while failing. `m11_savestate_roundtrip` calls it with `cap = 8`.
> * `gb_load` returns false for a foreign buffer **and mutates nothing**. The test checks
>   both.
> * The payload must include `ppu.framebuffer` (checked at three sample points) and
>   `total_ticks` (checked exactly), because those two catch "I forgot a whole component".
> * Anything you serialise must also be *restored*, and in the same order. An asymmetric
>   field list is the single most common bug here; keep the write and read halves adjacent
>   in the file so a mismatch is visible.
> * Do not `memcpy` whole structs. `ppu_t` and `gb_t` contain padding, and `cart_t` holds
>   `u8 *rom`, `u8 *ram` and a `FILE *`-adjacent save path. Copy scalars and arrays
>   explicitly.
> * `savestate.c` already exists with the two stubs; the build globs `src/*.c`, so you do not
>   need to touch the build script for this lesson.
> * Restoring `cart.ram` must not free and reallocate — write into the existing buffer. A
>   load that reallocates leaves dangling pointers if anything cached one.

---

## 2. Your task — 55 min

Work in `gb/src/savestate.c`.

1. **The cursor** (15 min). A writer and a reader with bounds checks and a single flag each.
   No `fwrite`; you are serialising to a memory buffer.
2. **The header** (10 min). Magic, version, payload length. Validate all three in `gb_load`.
3. **The field list** (20 min). Walk `gb_t` top to bottom and write every mutable field, in
   the same order as the struct, with a comment naming the component. Then the read half in
   the same order.
4. **Run the test** (10 min):
   ```
   .\gb\build.cmd -Test m11_savestate_roundtrip
   ```
   When it passes, `cap = 8` returns 0 and `DE AD BE EF` is refused.

---

## 3. Prove it — 15 min

```
.\gb\build.cmd -Test m11_savestate_roundtrip
```
```
ALL GREEN
```
Then prove it end to end, which is the only check that catches a forgotten *component*
rather than a forgotten field. Add two flags to `main.c` next to the existing ones:

```
--save-state FILE    write a state after the run
--load-state FILE    load a state before the first frame
```

Now use the strongest property a save state has: **a complete state reproduces the future
exactly.** A machine that runs 360 frames from cold and a machine that runs 300 frames from
cold, saves, loads that state, and runs 60 more must be in the *same state* at the end —
and therefore produce the same frame:

```
:: A: 360 frames straight from cold
.\gb\build\gbemu.exe --rom <game>.gb --frames 360 --dump-frame build\a.bmp

:: B: 300 frames, save, then load that state and run the remaining 60
.\gb\build\gbemu.exe --rom <game>.gb --frames 300 --save-state build\s300.state
.\gb\build\gbemu.exe --rom <game>.gb --load-state build\s300.state --frames 60 --dump-frame build\b.bmp

python tools\compare_bmp.py build\a.bmp build\b.bmp
```
```
0 differing pixels
```

That is the whole point of the lesson in one command. If `a.bmp` and `b.bmp` differ, some
piece of machine state is missing from your format — and the *shape* of the difference (which
`compare_bmp.py` names for you: a band, a block, or a whole region) usually says which
component. A difference in the sprite area points at OAM; a whole-screen smear points at
`SCX`/`SCY` or the PPU's clock state; a correct image with the wrong colours points at the
palettes.

One more, as a corollary: load the same state twice and it must behave identically both
times. A save state that works only on the first load is a state that is being mutated by
the act of loading it.

---

## 4. If it fails

| Symptom | Cause | Find it |
| --- | --- | --- |
| The test fails on `ppu.dot` or `ppu.mode` | you serialised the PPU registers but not its clock state | `dot`, `mode`, `window_line`, `stat_line` |
| `total_ticks` is wrong | you forgot the machine counters, or restored the struct in a different order | write and read the same list |
| The framebuffer is stale | you skipped it as "it gets redrawn" | 23,040 bytes, include it |
| `cap = 8` does not return 0, or crashes | you write before checking the bound | one cursor, one overflow flag |
| `DE AD BE EF` is accepted | no magic/version check | validate the header first |
| A field is right, the next is garbage | a write/read asymmetry | keep the halves adjacent |
| The game misbehaves only after loading | you restored RAM by reallocating, or forgot an MBC latch | write into the existing buffer; include the latches |
| It works until L35 | you added a field and forgot `savestate.c` | that is the lesson, not a surprise |

---

## 5. Done when

- [ ] `m11_savestate_roundtrip` is green, including both refusal cases
- [ ] `--save-state` / `--load-state` exist and two runs that load the same state produce byte-identical frames
- [ ] You can list, from memory, the three things a save state must NOT contain and why
- [ ] `NOTES.md` records "when adding machine state, add it to savestate.c"
- [ ] `-Test` shows no regression
- [ ] Commit message like `feat(savestate): versioned snapshot of the whole machine (L32)`

---

## 6. Optional, only if you have time

* Support several save slots per game (`<rom>.s0`, `.s1`, ...) and a hotkey to cycle them.
  The format does not change; only the path does.
* `docs/06-verification-and-tooling.md` §6.5 — the debugging section, which is where save
  states earn their keep: load a state, step, and reproduce a bug in seconds instead of
  replaying for minutes.

Next: **[L33 — The disassembler and the debugger](L33-the-disassembler-and-the-debugger.md)**
