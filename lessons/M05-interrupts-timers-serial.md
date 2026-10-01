# M05 - Interrupts, timers and serial

**Goal** - Wire the interrupt controller (`IME`/`IF`/`IE`, priority, `EI` delay, `HALT` and the halt bug), the `DIV`/`TIMA`/`TMA`/`TAC` timer, and a serial port accurate enough that blargg's `cpu_instrs` prints `Passed`.

**Estimated effort** - 10-14 h over 2-3 sessions. The serial port takes an hour; the interrupt timing and the halt paths take the rest.

## Read first

- `docs/02-cpu.md` section 2.6 - the dispatch sequence and the `EI`/`DI`/`RETI`/`HALT` rules. It owns the CPU side.
- `docs/01-orientation.md` section 1.5 - the milestone table, and note that the `cpu_instrs` gate is milestone 3 there.
- `docs/04-ppu-and-peripherals.md` section 4.5 - the serial port, including the shortcut every test ROM relies on. Section 4.1 for the VBlank interrupt you will wire in M06.
- `docs/06-verification-and-tooling.md` section 6.3 - it owns how to obtain and run blargg's ROMs and how to read their output. Follow it; do not improvise a different harness.
- `docs/03-memory-and-cartridge.md` section 3.2 - the I/O-side-effect rule, and `gb/include/gb/bus.h` for `GB_IF(gb)` / `GB_IE(gb)` and `bus_request_interrupt()` (given, in `gb/src/bus.c`).
- `gb/include/gb/timer.h` and `gb/include/gb/serial.h` - the provided state, including `serial_t.bit_timer` (512 per bit) and `timer_t.div_counter` (a 16-bit counter, DIV is its high byte). `gb/include/gb/gb.h` for `serial_out`.
- Pan Docs: "Interrupts", "Timer and Divider Registers", "Serial Data Transfer (Link Cable)".

## Why this milestone exists

Interrupts are the moment your emulator stops being a calculator and becomes a machine that reacts. Everything after this - VBlank, joypad, serial, audio - is "a peripheral raises a bit in `IF`", so the dispatch path you write here is the contract every later component talks to. The two rules that cost people days are the `EI` one-instruction delay and the `HALT` semantics, because both are invisible until a game's main loop spins on `HALT` and never wakes, or dispatches an interrupt one instruction early. The timer is your first free-running counter, and it is also the `DIV` register every timing test reads: keeping one 16-bit counter and deriving both `DIV` and the `TIMA` clock from it is the difference between a timer that works and a timer that is two counters away from being right. The serial port is the milestone's real reward: it is your `printf`, the mechanism every test ROM uses to talk to you.

## Deliverable contract

`gb/src/cpu.c` / `gb/src/opcodes.c`:

- Interrupt dispatch at the **top of `cpu_step()`**, before any opcode fetch:
  `bit = lowest set bit of (GB_IE(gb) & GB_IF(gb) & GB_INT_MASK)`; if `gb->cpu.ime` is true and a bit exists: clear that `IF` bit, `ime = false`, push `PC` (`SP-1` gets high, `SP-2` gets low), `PC = GB_INT_VECTOR(bit)`, return **20** T-cycles.
- `0xF3` `DI`: `ime = false`, `ime_pending = false`.
- `0xFB` `EI`: `ime_pending = true`. Promote `ime_pending` to `ime` **after the next instruction completes** - not at the end of `EI` itself. An interrupt must not dispatch between `EI` and that next instruction.
- `0xD9` `RETI`: pop `PC` and set `ime = true` immediately (no delay).
- `0x76` `HALT`: 4 T-cycles, then behave as follows:

| `IE & IF & 0x1F` at execution | `IME` | Behaviour |
| --- | --- | --- |
| zero | either | `halted = true`; wake when `IE & IF & 0x1F` becomes non-zero |
| non-zero | 1 | no halt; the interrupt dispatches on the next `cpu_step` |
| non-zero | 0 | **halt bug**: no halt, and the next fetch does not advance `PC` (the following byte is read twice) |

`gb/src/timer.c` (clean divider model - the DMG reload delay and `TAC` glitch are **M09**, not this milestone):

```c
void timer_tick (gb_t *gb, u32 tcycles);
void timer_reset(gb_t *gb);   /* extend the given skeleton */
```

The internal counter advances **once per T-cycle**; `DIV` is its high byte, so `DIV` visibly changes every 256 T-cycles (16384 Hz). The `TAC` rate select then clocks `TIMA` off a specific bit of that same counter, which is the only model that can express the 16 T-cycle period below. The comment in the provided `gb/include/gb/timer.h` states the same thing: advance the counter once per T-cycle, derive `DIV` from bit 8 upward, and do not store a separate `DIV` byte.

| Register | Address | Behaviour you implement |
| --- | --- | --- |
| `DIV` | `FF04` | read = `div_counter >> 8`; **write resets `div_counter` to 0** |
| `TIMA` | `FF05` | `timer.tima`; increments on the selected divider bit's falling edge; on `0xFF -> 0x00` reload from `TMA` and request `GB_INT_TIMER` |
| `TMA` | `FF06` | `timer.tma`, the reload value |
| `TAC` | `FF07` | bit 2 enable, bits 1-0 rate select; only these 3 bits are stored |

`TAC` rate select, and the `DIV`-counter bit that clocks `TIMA`:

| `TAC & 3` | `TIMA` frequency | `DIV`-counter bit | T-cycles per `TIMA` increment |
| --- | --- | --- | --- |
| 0 | 4096 Hz | bit 9 | 1024 |
| 1 | 262144 Hz | bit 3 | 16 |
| 2 | 65536 Hz | bit 5 | 64 |
| 3 | 16384 Hz | bit 7 | 256 |

`gb/src/serial.c`:

```c
void serial_tick (gb_t *gb, u32 tcycles);
void serial_reset(gb_t *gb);   /* extend the given skeleton */
```

Writing `SC` (`FF02`) as `0x81` starts an internal-clock transfer: shift `SB` out at 8192 Hz (one bit per 512 T-cycles, 4096 T per byte). On the last bit: call `gb_serial_byte(gb, sb)` with the transmitted byte, set `SB = 0xFF` (no link partner), clear `SC` bit 7, and request `GB_INT_SERIAL`. `docs/04` section 4.5 documents the immediate-completion shortcut that every test ROM tolerates; the per-bit implementation above also passes and is what the provided `bit_timer`/`bits_left` fields are for. Pick one and put the choice in `NOTES.md`.

`gb/src/bus.c` now owns the live register wiring (the M01 skeleton returned storage bytes):

```
FF00 -> joypad_read(gb)              FF01/FF02 -> serial.sb / serial.sc
FF04 -> timer.div_counter >> 8       FF05/FF06/FF07 -> timer.tima/tma/tac (writes routed to timer)
FF0F -> GB_IF(gb), only bits 0-4 writable, bits 5-7 read as 1
FFFF -> gb->bus.ie
```

## Work order

1. `DI`, `EI`, `RETI`, and the `ime_pending` promotion. Run `.\gb\build.cmd -Test m05_ime_ei_delay` before there is anything to dispatch; the test checks the delay itself.
2. Dispatch at the top of `cpu_step`, with the "lowest set bit" priority. Run `m05_interrupt_dispatch`.
3. `HALT`, then the halt bug. Run `m05_halt_wake` and `m05_halt_bug`.
4. Wire `FF0F` and `FFFF` in `bus_read`/`bus_write` (including the read-only upper bits of `IF`), then run the three interrupt tests again through `bus_write` instead of direct field pokes.
5. `timer_tick`: advance `div_counter` once per T-cycle (`+= tcycles`), derive `DIV` from the high byte, and implement the `DIV` write reset. Run `m05_timer_div_rate`.
6. `TIMA`: detect the falling edge of the selected `DIV`-counter bit (store the previous bit value, or count down a period), increment, overflow-reload from `TMA`, and `bus_request_interrupt(gb, GB_INT_TIMER)`. Run `m05_timer_tima_overflow` and `m05_timer_tma_reload`.
7. Wire `FF04-FF07` in the bus and re-run the three timer tests through `bus_write`/`bus_read` only.
8. `serial_tick` with a 512-cycle bit timer, the completion path, and `gb_serial_byte`. Run `m05_serial_emits_byte`.
9. Run `.\gb\build\gbemu.exe --rom <a small ROM you trust> --serial - --max-cycles 50000000` and confirm serial output reaches the terminal without an explicit flush call in the ROM.
10. The gate: `cpu_instrs`. Run it and read the verdict (see below). Debug in milestone order: instruction bugs first (M02-M04), then interrupts, then timer/DIV.
11. Full filter and regression: `.\gb\build.cmd -Test m05`, then `.\gb\build.cmd -Test m0`.

## Acceptance tests

```
.\gb\build.cmd -Test m05
.\gb\build.cmd -Test m05_timer_div_rate       # one at a time while debugging
.\gb\build\gbemu.exe --rom roms\cpu_instrs.gb --serial - --max-cycles 1000000000
```

Pass criterion: `m05_interrupt_dispatch`, `m05_ime_ei_delay`, `m05_halt_wake`, `m05_halt_bug`, `m05_timer_div_rate`, `m05_timer_tima_overflow`, `m05_timer_tma_reload`, `m05_serial_emits_byte` all PASS, and the `cpu_instrs` run prints `Passed` as its final verdict line.

**The gate, and how to read it.** `cpu_instrs` is a test ROM: it writes one character to `SB`, starts a transfer by writing `0x81` to `SC`, and waits for the transfer to finish, once per character of its report. `--serial -` routes every completed byte to `gb_serial_byte()` and then to stdout, so the ROM's text is your output. Expect a header line, per-sub-test progress, and a final `Passed`; a failure names the sub-test that failed, printed by the ROM itself, and the same archive contains that sub-test as a standalone `.gb` - run it alone and the failure is isolated. If the terminal shows nothing at all, the serial path or the flush is wrong, not the CPU: a ROM that produces no output is much more likely to be a `SC` handling bug than a perfect CPU. `docs/06-verification-and-tooling.md` section 6.3 has the canonical filenames and invocation; use it.

If `cpu_instrs` fails only in its timing sub-test, note the exact sub-test in `NOTES.md`, proceed to M06, and come back for it in M09 - that is what M09 exists for. A failure in the interrupt or instruction sub-tests is a real M02-M05 bug; do not defer those.

## Common traps

- Dispatching after the fetch instead of before. Symptom: the interrupt handler's return address is one instruction too late, `PC` is off by the length of the instruction that should not have run, and the game crashes inside its own handler. Cause: checking `IME & IE & IF` at the bottom of `cpu_step` after executing an opcode. Detect: `m05_interrupt_dispatch` - assert the pushed return address equals the PC of the instruction that had *not* yet executed.
- `IF` acknowledgement done in the wrong place, or the wrong bit chosen. Symptom A: the same interrupt re-dispatches forever and `SP` walks down through memory (bit never cleared). Symptom B: a second simultaneous interrupt is lost (two bits cleared, or the highest-priority bit was not the one serviced). Cause: clearing in the peripheral raise path instead of at acknowledgement, or scanning bits from 7 downwards. Detect: set `IF = IE = 0x1F`, dispatch once, assert `PC == 0x0040` and `GB_IF(gb) == 0x1E` - exactly one bit cleared, and it is bit 0.
- `IME` lifetime wrong. Symptom A: `EI` immediately followed by `DI` still dispatches an interrupt between them, and `HALT` wakes one instruction early. Symptom B: nested interrupts run without an `EI`, and the stack overflows. Cause A: setting `ime` inside the `EI` handler instead of promoting `ime_pending` after the next instruction. Cause B: forgetting `ime = false` at acknowledgement. Detect: `m05_ime_ei_delay`, plus assert `gb->cpu.ime == false` in the newly entered handler.
- `HALT` semantics wrong. Symptom A: a game that halts for VBlank wakes thousands of times a second and burns host CPU. Symptom B: `m05_halt_bug` fails and a game that halts with `IME=0` and a pending interrupt desynchronises by one instruction. Cause A: testing `IF != 0` instead of `(IE & IF & GB_INT_MASK) != 0`. Cause B: treating `IE & IF != 0` as "stay halted" instead of "do not halt, and the next fetch does not advance PC". Detect: `m05_halt_wake` with a masked bit set, and `m05_halt_bug` checking that the byte after `HALT` is fetched, executed and then fetched again.
- Divider counter model wrong. Symptom: `DIV` looks fine but `TAC = 01` (262144 Hz) never reaches its 16 T-cycle rate, or `DIV` and `TIMA` drift apart. Cause: advancing the counter once per 256 T-cycles (as the comment in `gb/include/gb/timer.h` implies) instead of once per T-cycle, or keeping a `u8 div` alongside a separate `TIMA` accumulator. Detect: after a `DIV` write, `div_counter == 0`; then measure that `DIV` (the read value) changes every 256 T-cycles *and* `TIMA` ticks every 1024/16/64/256 T-cycles for `TAC & 3 = 0/1/2/3`.
- `TIMA` overflow not reloading `TMA`. Symptom: `TIMA` sits at `0x00` after overflow, `m05_timer_tma_reload` fails, and `TMA`-driven sound or `r`-register randomness misbehaves. Cause: incrementing a `u8` (it wraps silently) and forgetting the reload, or forgetting `bus_request_interrupt(gb, GB_INT_TIMER)`. Detect: set `TIMA = 0xFF`, `TMA = 0x42`, tick to the selected bit's falling edge, assert `TIMA == 0x42` and that `GB_IF(gb) & GB_INT_TIMER` is set.
- Serial transfer never "completes". Symptom: a test ROM's wait loop spins forever and you see no output at all, or output appears but the ROM never advances past its first character. Cause: finishing the shift without clearing `SC` bit 7 and requesting `GB_INT_SERIAL`. Detect: `m05_serial_emits_byte` asserts `SC` bit 7 is clear and `GB_IF(gb) & GB_INT_SERIAL` after completion; if you chose the immediate shortcut from `docs/04` section 4.5, the same assertions must hold right after the write.
- Buffered serial output. Symptom: `--serial -` prints nothing until the emulator exits, so a hung ROM looks like a ROM that produced no output, and you debug the CPU instead of the harness. Cause: stdout buffering in the `gb_serial_byte` path. Detect: run a ROM that prints one byte and then idles; if the byte appears only at exit, the hook needs `fflush(stdout)`.

## Hint ladder

### H1

- Where in `cpu_step` must the dispatch check live, and what does that ordering mean for the instruction that was about to execute?
- Which registers does the CPU modify when it acknowledges an interrupt, and which one wins when several are pending?
- What are the two different reasons `HALT` can stop the CPU, and what wakes it in each case? What happens if the wake condition is met while `IME` is 0?
- Why must `DIV` be derived from a 16-bit counter rather than stored as a byte? What does a `DIV` write reset?
- Which bit of the divider clocks `TIMA`, and how does that bit follow from the `TAC` rate rather than from the frequency in Hz?

### H2

- Technique: implement dispatch as one function that returns 20 or 0, called at the top of `cpu_step`. Everything else (peripherals, `HALT`, the halt bug) then has exactly one place to interact with.
- Technique for `EI`: promote `ime_pending` at the *end* of `cpu_step`, after the instruction is complete, and only if the instruction was not `DI`. Two lines, and the ordering is the whole feature.
- Technique for the timer: store the previous value of the selected divider bit (or a countdown), not a second frequency generator. `timer_tick` becomes: advance `div_counter`, compute the new bit, and if the old bit was 1 and the new is 0, increment `TIMA`.
- Technique for the gate: run `cpu_instrs` with `--trace` from the start once. The first sub-test it fails tells you which milestone to revisit; the trace tells you the instruction. Do not debug `cpu_instrs` by staring at serial output.
- Spec pointers: docs/02 section 2.6 (dispatch, `EI` delay, `HALT`), docs/04 section 4.5 (serial, including the shortcut), docs/06 section 6.3 (obtaining and running test ROMs).

### H3

- Dispatch, in order: `pending = GB_IE(gb) & GB_IF(gb) & GB_INT_MASK`; if `ime && pending`: `bit` = index of the lowest set bit; `GB_IF(gb) &= ~(1 << bit)`; `ime = false`; push `PC` (high byte at `SP-1`, low at `SP-2`); `PC = 0x0040 + bit * 8` (the `GB_INT_VECTOR(bit)` macro); return 20.
- Vectors by bit: 0 VBlank `0x0040`, 1 STAT `0x0048`, 2 Timer `0x0050`, 3 Serial `0x0058`, 4 Joypad `0x0060`.
- `DIV` read: `timer.div_counter >> 8`; `DIV` write: `timer.div_counter = 0`. `TAC` rates: `0 -> 1024` T per `TIMA` increment (bit 9), `1 -> 16` (bit 3), `2 -> 64` (bit 5), `3 -> 256` (bit 7); enable is `TAC` bit 2.
- `TIMA` overflow: on `0xFF -> 0x00`, `timer.tima = timer.tma` and `bus_request_interrupt(gb, GB_INT_TIMER)`. The one-M-cycle delay between overflow and reload, and the "write `TIMA` during the reload window is ignored" behaviour, are M09.
- Serial timing: 8 bits at 512 T-cycles each = 4096 T per byte; completion means `gb_serial_byte(gb, serial.sb)`, `SB = 0xFF`, `SC` bit 7 clear, `bus_request_interrupt(gb, GB_INT_SERIAL)`.

## Done when

- `.\gb\build.cmd -Test m05` reports 8/8 PASS and `m0` shows no M01-M04 regressions.
- With `IF = IE = 0x1F` and `IME` set, one `cpu_step` dispatch reaches `0x0040` in 20 T-cycles and clears exactly one `IF` bit.
- `EI` + `DI` back-to-back never dispatches; `HALT` with a pending unmasked interrupt does not stay halted; `HALT` with `IME=0` and a pending interrupt produces the halt bug (the next byte is fetched twice).
- `DIV` at `FF04` reads `div_counter >> 8`, and writing it zeroes the counter; `TIMA` increments at exactly 16/64/256/1024 T-cycles for the four `TAC` selections.
- `cpu_instrs` prints `Passed` on `--serial -`, and you can name which sub-tests were failing before it did.
- `NOTES.md` records your serial model (per-bit or immediate) and any `cpu_instrs` sub-test you deliberately deferred to M09, with the reason.

## Stretch

- Implement `STOP` properly: the two-byte form, and (for the `stopped` field) a machine that resumes only on a joypad edge. It costs little now and CGB speed switching in M12 will want the flag.
- Add the `TAC` write glitch: recompute the divider clock immediately on a `TAC` enable or rate change and increment `TIMA` if the selected bit is currently high. It is a known DMG behaviour and later test ROMs exercise it; keep it behind a comment so M09 can adopt it deliberately.
- Build a serial logger that timestamps and prefixes each byte (`[12345678] cpu_instrs\n`) into the `--serial FILE` path. When a ROM prints 4000 characters, the timestamp tells you where it stalled.

## Commit

```
feat(cpu,timer,serial): IF/IE/IME dispatch, EI delay, HALT + halt bug, DIV/TIMA/TMA/TAC, serial out; cpu_instrs Passed (M05 green)
```
