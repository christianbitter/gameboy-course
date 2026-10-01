# M11 - Polish: save states, a real disassembler, and a debugger

**Goal** - Make the emulator inspectable and restartable: versioned save states, a disassembler that decodes every opcode, a working tracer, breakpoints, fast-forward and a real frontend.

**Estimated effort** - 10-14 h (save states ~3 h, disassembler ~4 h, debugger ~4 h, frontend and input ~3 h). Mostly mechanical work, full of off-by-one landmines.

## Read first

- `gb/include/gb/debug.h` - the provided tracer ring (`GB_TRACE_DEPTH` 256, `trace_entry_t`, `gb_trace_record`, `gb_trace_print_last`) and the `gb_disasm` signature you must replace.
- `gb/src/debug.c` - the placeholder `gb_disasm` (it prints raw hex bytes and reads straight from `gb->cart.rom`) and the tracer that calls it with `e->text`, which is **28 bytes**.
- `gb/src/gb.c` - where `gb_trace_record` is called (once per instruction, by `gb_step`), and the `total_ticks` accounting your save states must preserve.
- `gb/include/gb/gb.h` - the whole `gb_t` you are serialising, and the provided host services `gb_write_bmp` / `gb_write_ppm` / `GB_SHADES`.
- `gb/src/main.c` - the provided CLI. M11 is where `--save-state` / `--load-state` and the window appear; today `--headless` is the only mode.
- `docs/06-verification-and-tooling.md` sections 6.2, 6.4 and 6.6 for the test conventions, the tracer, and the CLI contract.
- `docs/01-orientation.md` 1.4 (determinism) and 1.7 (tracer) - this milestone is where those two pay off.
- `docs/02-cpu.md` opcode tables: your disassembler is their executable form, so a decoding hole is a table hole.
- `gb/tests/harness.h` for the fixture style. SDL2 via MSYS2 (`pacman -S mingw-w64-x86_64-SDL2`) or vcpkg; `ffmpeg` if you stay headless.

## Why this milestone exists

Save states turn every bug into a reproducible one: snapshot at frame 899, step, watch the first wrong cycle. A disassembler turns the tracer from `CD 9A 01` into `CALL $019A` and simultaneously audits your opcode table, because an instruction it cannot decode is an instruction you cannot execute. Breakpoints turn "the game hangs somewhere" into "execution reached 0x01A7, here are the registers". A real window with real input is the difference between running test ROMs and actually using the thing you built. None of this is emulation, and all of it decides how fast the remaining milestones go. Build it now, because M12 is where you find out whether it was good enough.

## Deliverable contract

Save states: `gb/include/gb/savestate.h` does not exist yet; create it and `gb/src/savestate.c` with exactly these signatures.

```c
size_t gb_save(const gb_t *gb, u8 *buf, size_t cap);   /* bytes written, 0 on failure */
bool   gb_load(gb_t *gb, const u8 *buf, size_t len);   /* false on mismatch, no partial mutation */
```

Disassembler: `gb_disasm` already has its signature and a placeholder body in the provided `debug.c`. Replace the body; do not change the prototype.

```c
void gb_disasm(gb_t *gb, u16 addr, char out[32]);   /* fixed: void, out[32], no return */
```

Disassembler requirements:

- All 256 base opcodes and all 256 CB-prefixed ones decode to real mnemonics. Illegal opcodes get a named form (`DB $DB` style), never "unknown", and never the raw-hex placeholder.
- Operand vocabulary, fixed: `n8`, `n16` little-endian, `e8` resolved to an absolute address, `(HL)`, `(HL+)`, `(HL-)`, `$FF00+n`, `(n16)`, register names.
- Read operand bytes with a peek that has no side effects and no cycle cost: `gb_disasm` must never call `bus_read`. The placeholder shows the pattern - read the region directly (`gb->cart.rom` below 0x8000, and the right array for VRAM/WRAM/HRAM so the debugger can decode code copied into RAM).
- **Buffer trap:** the tracer passes `e->text` (28 bytes) while the prototype says 32. Anything longer than 27 characters plus the terminator writes into the next struct field. Truncate to a documented limit and assert it in the test.
- The tracer ring is provided: `gb_trace_record` is called once per instruction from `gb_step`. Your job is the `text` it fills, plus proving the ring keeps the last N.

Debugger surface (names yours, behaviour not):

- `step` - exactly one instruction, then repaint the registers.
- `regs`, `x ADDR [len]`, plus VRAM/OAM views.
- Breakpoints on PC, on a memory read, and on a memory write, with hit counts and a one-shot mode. Implement them in `bus_read`/`bus_write` and at instruction fetch - the provided `--break-op` in `main.c` is a stopping point, not the feature.
- CLI additions in `main.c`: `--save-state FILE`, `--load-state FILE`. Keep the existing flags working.
- Frontend: SDL2 window, 160x144 texture scaled by an integer factor, ~60 Hz pacing from a wall clock (the DMG frame rate is 59.7275 Hz), keyboard mapped to the joypad matrix, fast-forward on a held key. `--headless` must run the same emulation with `gb_write_bmp`/`gb_write_ppm` and no SDL.

Save state format:

```
+0  magic "GBST"        4 bytes
+4  version (u16)       refuse anything you do not recognise
+6  total length (u32)  must equal len passed to gb_load
+10 fields in fixed order, little-endian, one writer function per component
... cart RAM last: ram_size bytes
```

Must cover: CPU registers and `ime`, `ime_pending`, `halted`, `halt_bug`, `stopped`; `bus.vram`, `wram`, `oam`, `hram`, `io`, `ie` and the four DMA fields; every `ppu_t` field including `dot`, `mode`, `stat_line`, `window_line`, the framebuffer and `frame_ready`; every `timer_t` field including the full 16-bit `div_counter`, `reload_delay` and `overflow_pending`; the `cart_t` latch fields plus the whole of `cart.ram`, and any file-static RTC state you had to introduce in M08 because `cart_t` has no RTC fields (a save state that loses the clock is not a round trip); joypad, serial and APU state; `total_ticks` and `frame_count`. Not the ROM, and not host configuration (`serial_out`, `trace.file`, `headless`, paths). Rule of thumb: if a field is not derivable from registers the CPU can read back, it belongs in the file, because M09's timing quirks must survive a save/load byte-for-byte.

## Work order

1. Write the save format list on paper first: fields, order, widths, version. Then implement the writer and the reader, one component at a time.
2. `gb_load` deserialises into a scratch `gb_t` and commits only on success. Run `gbemu_tests m11_savestate_roundtrip`.
3. Verify by hand: run 600 frames, save, run 60 more, hash the framebuffer and register dump; load; run the same 60; compare both hashes.
4. Load that state in a **second process** from a fresh `gb_t` with the same ROM. This is the test that catches pointers and absolute addresses in your format.
5. Disassembler: base table, then CB table, then illegal opcodes. Write the coverage test first and let it name what you have not handled. Run `m11_disassembler_covers_all_ops`.
6. Cross-check: disassemble 4 KiB of a known ROM and diff against `mgbdis` or `rgbds`. Every difference is a table bug.
7. Tracer: make sure `text` renders real mnemonics within the 28-byte buffer, then run `m11_trace_ring_keeps_last_n`.
8. Debugger: pause, step, registers, memory, breakpoints. Prove with the tracer that a step advances exactly one instruction and a write breakpoint stops at the right PC.
9. Frontend: SDL2 plus input and fast-forward. Get `--headless` byte-identical to the windowed path for the same 300 frames, then add scaling.
10. Update the usage text in `main.c` so every documented flag does something.

## Acceptance tests

```
gbemu_tests m11_                          # add tests/t_m11_debug.c; expect "ALL GREEN", exit 0
gbemu --rom <rom>.gb --save-state s.bin --frames 600 --headless
gbemu --rom <rom>.gb --load-state s.bin --frames 60 --dump-frame after.bmp --headless
```

Tests the student must write: `m11_savestate_roundtrip`, `m11_disassembler_covers_all_ops`, `m11_trace_ring_keeps_last_n`.

Pass criteria: all three PASS with `ALL GREEN`; the round trip is byte-identical across two separate processes including cart RAM; all 512 encodings decode to a mnemonic with sane operand widths and none renders the raw-hex placeholder; the disassembly diffs clean against `mgbdis` on 4 KiB; the tracer holds exactly the last 256 entries in order.

## Common traps

- `memcpy` of `gb_t`. Symptom: save/load works until you add a field, then yesterday's file crashes the emulator or restores a stale pointer. Cause: pointers, host padding and `FILE *`/SDL handles in the struct, and no version tag. Detect: `m11_savestate_roundtrip` loading into a fresh machine in a second process.
- Save state missing cart RAM or the MBC latches. Symptom: after a load the game is in the right place with a reset inventory. Cause: serialising only CPU/PPU/timer state. Detect: include `cart.ram` and `rom_bank`/`ram_bank`/`mode`/`bank_hi` in the round-trip hash.
- `gb_disasm` writing into `out[32]` when the tracer passed 28 bytes. Symptom: one trace entry's text corrupts the neighbouring fields, or the trace output looks shifted at long instructions. Cause: no truncation and a `snprintf` limit of 32. Detect: assert `strlen(text) < 28` for all 512 encodings in the coverage test.
- Disassembler calling the real `bus_read`. Symptom: opening the debugger changes the machine - LY moves, an I/O read has side effects, timing drifts. Cause: decoding through the access-stepped bus. Detect: hash the machine state before and after 512 disassemble calls; it must be identical.
- Wrong operand length for relative jumps. Symptom: the debugger view desynchronises after the first `JR` and prints plausible garbage. Cause: `e8` handled as 2 bytes, or the target computed as `addr + e8` instead of `addr + 2 + (s8)e8`. Detect: the coverage test plus the `mgbdis` diff.
- Illegal opcodes rendered as something harmless. Symptom: the tracer hides the exact bug you are hunting, and `cpu_fatal` for illegal opcodes never fires. Cause: a table hole filled with NOP. Detect: enumerate the 11 illegal opcodes and assert both the disassembly text and the guest fault.
- Breakpoint hooked after the access. Symptom: a read breakpoint fires one access late and the register dump is post-instruction. Cause: checking after the read was serviced. Detect: breakpoint on a read of `0xFF40`, assert the state is pre-instruction.
- Fast-forward implemented by skipping emulation. Symptom: games run at a different speed or drift out of sync after releasing the key. Cause: whole frames skipped instead of presentation and pacing skipped. Detect: compare 60 fast-forwarded frames' framebuffer hash against 60 normally paced frames.

## Hint ladder

### H1

- What in `gb_t` cannot be written to disk as bytes, and what in your format changes the day you add a field? How will a file on disk prove it came from the current build?
- How many distinct instructions exist once the CB block is included, and what does your table do for the handful with no mnemonic?
- `gb_disasm` writes into a 32-byte buffer but is called with a 28-byte one. What is the largest instruction text you may emit, and what is the count of bytes you must never exceed?

### H2

- Technique: define the save format list-first and version it; deserialise into a scratch machine and commit with one assignment. Build the coverage test before the disassembler is complete so it tells you which encoding is still missing.
- Spec pointers: docs/06 for the CLI and tracer conventions, docs/02's opcode tables for the decode, Pan Docs for operand widths, `mgbdis` output as ground truth.
- Debug technique: drive the debugger from the test binary too, not only from the CLI. `m11_disassembler_covers_all_ops` and `m11_trace_ring_keeps_last_n` are ordinary harness tests, so they run in CI-style batches with everything else.
- Frontend technique: make `--headless` byte-identical to the windowed path first, then add scaling and input. Presentation must never touch machine state.

### H3

- Little-endian fields: `low | (high << 8)`. PC-relative: an `e8` operand resolves to `addr + 2 + (s8)e8`. `$FF00 + n` is `LDH`; the CB block's meaning comes from the base table's second half.
- The provided ring: write at `head`, `head = (head + 1) % GB_TRACE_DEPTH`, increment `count` up to the depth, and print from `(head + DEPTH - n) % DEPTH` for the last n - that is already `gb_trace_print_last`, so test it rather than reimplementing it.
- Round-trip recipe: frame 600, `gb_save` to `buf1`; run 60 frames and hash framebuffer plus registers; `gb_load(buf1)`; run the same 60 frames; both hashes must match. Then repeat the load in a second process.
- For a write breakpoint the check belongs at the top of `bus_write`, before the peripheral sees the value; for a read breakpoint, before the read is serviced.

## Done when

- `gbemu_tests m11_` reports 3/3 passing and prints `ALL GREEN`.
- The save-state round trip is byte-identical across two separate processes, including cart RAM.
- The disassembler decodes 512/512 encodings, diffs clean against `mgbdis` on 4 KiB, and never exceeds the tracer's 28-byte text field.
- A write breakpoint on `0xFF40` stops before the write with correct registers; single-step advances exactly one instruction, proven from the tracer.
- 60 fast-forwarded frames hash identically to 60 normally paced frames.
- The SDL2 window runs a real game at roughly 60 fps, and `--headless` produces the same frames without SDL.

## Stretch

- Rewind: a ring of save states every 60 frames, plus one per frame for the last second, with a key to step backwards.
- A memory viewer that highlights every byte the last 256 traced instructions touched, plus expression watchpoints that stop when an address's byte changes or when PC enters a range, dumping the tracer automatically on hit.
- Deterministic replay: an input file of (cycle, buttons) events plus a start state producing byte-identical framebuffer hashes; that file becomes a regression test for M12.

## Commit

`git commit -am "debug: versioned save states, full disassembler, tracer text, breakpoints, SDL2 frontend"`
