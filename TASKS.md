# TASKS — your todo list

Rules for using this file:

* One milestone at a time, in order. The milestones are cumulative: `m03_*`
  cannot pass before `m01_*` does, because every CPU test loads a cartridge.
* Every item names an exact command. Run it before ticking the box.
* **A milestone is done when its tests are green *and* its gate passes.** The gate
  is what makes it true; the tests are what make it fast.
* When something is red, tick nothing and fix it. Never "come back to it in M09".
* Commit at every green milestone. Suggested messages are in each lesson.

Current state of the suite: `3 passed, 42 failed, 28 skipped` — that is the
starting line, not a problem.

---

## Right now — your first session (~90 minutes)

- [ ] Open the `gameboy-course` folder in VS Code, accept the recommended extensions, then `Ctrl+Shift+B` to build and `Ctrl+Shift+P` -> "Tasks: Run Task" -> `4 test: milestone (prompt)` to run a filter (details: `PREREQUISITES.md`)
- [ ] `python tools\check_env.py` reports every REQUIRED prerequisite as `[ OK ]` (the list behind it: `PREREQUISITES.md`)
- [ ] `.\gb\build.cmd` builds `gb\build\gbemu.exe` and `gb\build\gbemu_tests.exe`
- [ ] `.\gb\build.cmd -Test m00` reports exactly 3 tests, 3 PASS, exit 0
- [ ] `.\gb\build\gbemu.exe --help` prints the fixed CLI (`--rom --info --frames --max-cycles --dump-frame --ppm --serial --trace --no-boot-rom --headless`)
- [ ] Read `PREREQUISITES.md`, `README.md`, `docs/00-reading-path.md`, `docs/01-orientation.md`, `gb/README.md`
- [ ] `git init` + `.gitignore` (start from the template in M00's H3) + first commit
- [ ] Create `NOTES.md` and paste in: toolchain versions, the memory map from `docs/03` §3.2, the `gb_t` component list, the `gb_step` call graph, the register-ownership table from `gb/include/gb/bus.h`
- [ ] `.\gb\build.cmd -Test` and record the per-milestone counts in `NOTES.md`
- [ ] Open `lessons/M00-setup-and-harness.md` and finish its work order
- [ ] Start `lessons/M01-cartridge-and-bus.md`

---

## M01 — Cartridge and the address decoder

Tests: `m01_header_parse`, `m01_checksum`, `m01_cart_read_rom`, `m01_bus_wram_echo`,
`m01_bus_unmapped_ff`, `m01_bus_hram_ie`

- [ ] Read `docs/03-memory-and-cartridge.md` §3.1-3.4 and `reference/cheatsheet-io-registers.md`
- [ ] `cart_load`: validate loosely, copy the ROM into `gb->cart.rom`, parse the header into the named fields, allocate `gb->cart.ram` from the header RAM size
- [ ] Header checksum (`x = x - byte - 1` over `0x0134-0x014C`) -> `header_checksum_ok`; Nintendo logo compare -> `logo_ok`; **warn, never reject**
- [ ] `cart_read` for `0000-3FFF` and `4000-7FFF` (ROM-only: bank 0 mirrored)
- [ ] `cart_write` for `A000-BFFF`, ignored on ROM
- [ ] `bus_read`/`bus_write`: ROM, VRAM, cart RAM, WRAM, echo RAM, OAM, HRAM, IE, unmapped = `0xFF`
- [ ] `bus_read16`/`bus_write16` as two 8-bit accesses
- [ ] `FF46` OAM DMA as an instant 160-byte copy
- [ ] `bus_tick` calling `timer_tick`/`ppu_tick`/`serial_tick` (their stubs are fine)

```
.\gb\build.cmd -Test m01
.\gb\build\gbemu.exe --rom <any .gb> --info
```

**Gate:** 6/6 green, and `--info` matches a hexdump of `0x0134`/`0x0147`/`0x0148`/
`0x0149` for a ROM you can check by hand. Commit.

---

## M02 — CPU skeleton: fetch, decode, first opcodes

Tests: `m02_nop_advances_pc`, `m02_step_cycles_nop`, `m02_ld_r_d8`, `m02_ld_rr_d16`,
`m02_pc_after_2byte`, `m02_illegal_opcode_traps`

- [ ] Read `docs/02-cpu.md` §2.1, §2.3, §2.5 and `reference/cheatsheet-opcode-map.md`
- [ ] Decide your dispatch design (table of `{mnemonic, bytes, cycles, cycles_taken, fn}` vs one switch) and write it down in `NOTES.md`
- [ ] `cpu_fetch8`, `cpu_fetch16` (low byte first), `cpu_read_r8`, `cpu_write_r8` (index 6 = `(HL)`)
- [ ] `cpu_step`: fatal check, `ime_pending` resolution, interrupt dispatch hook, HALT hook, fetch, `op_execute`
- [ ] `op_execute` for `NOP`, `LD r,d8`, `LD rr,d16`, `JP a16`, `LD (a16),A`, `LD A,(a16)`
- [ ] All 11 illegal opcodes call `cpu_fatal()`; every other missing opcode calls `GB_UNIMPLEMENTED`
- [ ] Set `last_opcode`, `last_opcode_len`, `last_cycles`, `instruction_count`
- [ ] Add a self-check that no table entry is missing

```
.\gb\build.cmd -Test m02
.\gb\build.cmd -Test m01      # must still be green
```

**Gate:** 6/6 green plus no regression. Commit.

---

## M03 — 8-bit load matrix, ALU, flags, DAA

Tests: `m03_ld_r_r_matrix`, `m03_alu_add_flags`, `m03_alu_sub_flags`,
`m03_alu_adc_sbc_flags`, `m03_alu_logic_flags`, `m03_alu_cp_no_write`,
`m03_inc_dec_flags`, `m03_daa`, `m03_rotates_a`, `m03_hl_indirect_cycles`

- [ ] Read `docs/02-cpu.md` §2.4 in full. Twice. The `H` and `C` rules are the milestone.
- [ ] Fill `0x40-0x7F` as one loop over `dst`/`src` with `cpu_read_r8`/`cpu_write_r8`; `0x76` is `HALT` (M05)
- [ ] `0x80-0xBF` as one loop over the ALU op and the r8 operand
- [ ] `INC`/`DEC` r and `(HL)`: Z, N, H — and `C` untouched
- [ ] `RLCA`/`RRCA`/`RLA`/`RRA` (Z always 0), `CPL`, `SCF`, `CCF`
- [ ] `DAA`: high-nibble correction is `0x60`; the low-nibble test uses A **after** the high correction
- [ ] Remember `AND` sets `H = 1`; and the `(HL)` operand costs 4 extra T

```
.\gb\build.cmd -Test m03
.\gb\build.cmd -Test m0       # regression sweep M00-M06
```

**Gate:** 10/10 green, `m0` shows M01/M02 unregressed. Commit.

---

## M04 — Control flow, the stack, the CB block

Tests: `m04_jp_jr_conditions`, `m04_call_ret_stack`, `m04_push_pop_order`,
`m04_rst_vectors`, `m04_cb_rotates`, `m04_cb_bit_res_set`, `m04_program_loop_sum`

- [ ] Read `docs/02-cpu.md` §2.5 and the `CB` rows of `reference/cheatsheet-opcode-map.md`
- [ ] `JP`/`JR` with the four conditions and both cycle counts; `JR` is relative to the address *after* the instruction
- [ ] `CALL`/`RET`/`RETI`/`RST`, and `PUSH`/`POP` with the high byte at `SP-1`
- [ ] `POP AF` masks the low nibble of F
- [ ] `LD (a16),SP`, `ADD SP,e8`, `LD HL,SP+e8` — flags come from the low byte only
- [ ] 16-bit `INC`/`DEC` (no flags) and `ADD HL,rr` (H from bit 11, Z untouched)
- [ ] The whole `CB` page from the regularity rule, not 256 hand-written cases
- [ ] `m04_program_loop_sum` must print A=`0x37`

```
.\gb\build.cmd -Test m04
.\gb\build.cmd -Test m0
```

**Gate:** 7/7 green, no regression. Commit.

---

## M05 — Interrupts, HALT, timers, serial

Tests: `m05_interrupt_dispatch`, `m05_ime_ei_delay`, `m05_halt_wake`,
`m05_halt_bug`, `m05_timer_div_rate`, `m05_timer_tima_overflow`,
`m05_timer_tma_reload`, `m05_serial_emits_byte`

- [ ] Read `docs/02-cpu.md` §2.6, `docs/03` §3.2, `docs/04` §4.5, `docs/06` §6.3
- [ ] `IF`/`IE`/`IME`; dispatch before fetch: lowest bit wins, clear the IF bit, push PC, `PC = 0x40 + bit*8`, 20 T
- [ ] `EI` sets `ime_pending`; promote it after the *next* instruction retires. `DI` clears now. `RETI` sets `IME` now
- [ ] `HALT`: halt only while `IE & IF == 0`; clear `halted` when an interrupt is pending
- [ ] `timer_tick`: `div_counter` advances every T-cycle, `DIV = counter >> 8`, TIMA clocked by the **falling edge** of the TAC-selected bit, overflow -> TMA + IF bit 2
- [ ] `bus_write(FF04)` resets the counter; `bus_write(FF02, 0x81)` emits SB, sets `SC = 0x01`, raises IF bit 3
- [ ] `--break-op 40` (already implemented in `main.c`) stops on `LD B,B` for the mooneye tests

```
.\gb\build.cmd -Test m05
.\gb\build.cmd -Test m0
.\gb\build\gbemu.exe --rom roms\blargg\cpu_instrs\individual\01-special.gb --serial - --frames 4000
```

**Gate:** 8/8 green **and blargg `cpu_instrs` prints `Passed`** (all eleven
individual ROMs, or the combined ROM). This is the first real proof you wrote a
CPU. Commit.

---

## M06 — Frame loop, PPU skeleton, joypad, boot skip

Tests: `m06_frame_is_70224`, `m06_vblank_interrupt_fires`, `m06_joypad_matrix`,
`m06_boot_without_bootrom`, `m06_run_60_frames`

- [ ] Read `docs/04-ppu-and-peripherals.md` §4.1, §4.4, §4.6 and `reference/cheatsheet-flags-and-timing.md`
- [ ] `gb_apply_post_boot_state()` from the post-boot table (CPU half + LCDC/IE)
- [ ] `ppu_tick`: `dot` 0..455, LY 0..153, modes 2/3/0 with a fixed 172 for mode 3, mode 1 on lines 144-153
- [ ] `ppu.frame_ready = true` on entering mode 1; raise IF bit 0 (`gb_step()` lifts the flag to `gb->frame_ready`)
- [ ] `bus_write(FF40/FF44/FF45/FF46)` routing to `ppu`/`bus`
- [ ] `joypad_read`: the P14/P15 matrix, bits 6-7 read 1, high-to-low edge raises IF bit 4
- [ ] Run a real ROM headless and dump a frame

```
.\gb\build.cmd -Test m06
.\gb\build.cmd -Test m0
.\gb\build\gbemu.exe --rom <a real game>.gb --frames 600 --trace build\trace.txt --dump-frame build\frame.bmp
```

**Gate:** 5/5 green, no regression, and a real ROM runs 600 frames without
faulting (a black or garbage screen is fine — the picture is M07). Commit.

---

## M07 — PPU: background, window, sprites, STAT

Tests: you write the eight `m07_*` tests (replace the `TEST_TODO` stubs)

- [ ] Read `docs/04` §4.1-§4.3 and §4.7
- [ ] Palettes: `BGP`/`OBP0`/`OBP1` bits 2 per colour id -> shade
- [ ] BG tile decode: 2 bitplanes, `LCDC.4` signed/unsigned tile data, `SCX`/`SCY` wrap over 256x256
- [ ] Window: `WX-7`/`WY`, and a `window_line` counter that only advances when the window drew
- [ ] Sprites: `+16`/`+8` offsets, 8x16, flips, palettes, colour 0 transparent, 10 per line by X then OAM index, BG priority bit
- [ ] One scanline at a time — do **not** render the whole frame at VBlank
- [ ] STAT: live mode bits, LYC=LY, the four interrupt enables, `stat_line` rising edge
- [ ] LCD off (`LCDC.7 = 0`): mode 0, LY reads 0, blank screen

```
.\gb\build.cmd -Test m07_
.\gb\build\gbemu.exe --rom roms\dmg-acid2.gb --frames 30 --ppm build\acid2.ppm
.\gb\build\gbemu.exe --rom <a real game>.gb --frames 300 --dump-frame build\title.bmp
```

**Gate:** `dmg-acid2` shows the smiley/yes face with no stray pixels, and your
own ROM's title screen is recognisable. Commit.

---

## M08 — MBC1/2/3/5 and save RAM

Tests: you write the five `m08_*` tests

- [ ] Read `docs/03` §3.4-3.5
- [ ] Bank count from the header, masked everywhere; `cart_read`/`cart_write` dispatch on the MBC type
- [ ] MBC1: RAM enable, 5-bit bank with **0 -> 1**, the 2-bit register, mode select
- [ ] MBC2: address bit 8 switches enable/bank, 512 x 4-bit internal RAM
- [ ] MBC3: 7-bit bank, RAM bank vs RTC register select, latch sequence
- [ ] MBC5: 9-bit bank (bank 0 legal), RAM bank 0-15
- [ ] `cart_save` writes `<rom>.sav`; load it in `cart_load`; `ram_dirty` drives the save on exit

```
.\gb\build.cmd -Test m08_
.\gb\build\gbemu.exe --rom <a game with saves>.gb --frames 3600
```

**Gate:** a real game with battery RAM keeps its progress across two runs (same
`.sav`), and the file is byte-compatible with another emulator. Commit.

---

## M09 — Access-level timing and the awkward quirks

Tests: you write the five `m09_*` tests

- [ ] Read `docs/04` §4.1/§4.3 and `docs/06` §6.3
- [ ] Move to access-stepped timing: `bus_tick(4)` inside every `bus_read`/`bus_write`, and `cpu_step` returns only the **residual** cycles (see the note in `gb/src/gb.c` and `include/gb/cpu.h`)
- [ ] Model the CPU's internal/dummy cycles for the instructions that need them
- [ ] TIMA overflow/reload delay (4 T-cycles where TIMA keeps counting)
- [ ] OAM DMA: 160 M-cycles, CPU can only reach HRAM
- [ ] Mode 3 length varies; VRAM/OAM restricted access during mode 3; STAT blocking
- [ ] Keep the tracer on, and re-run *every* earlier filter after each change

```
.\gb\build.cmd -Test m0
.\gb\build\gbemu.exe --rom roms\blargg\instr_timing.gb --serial - --frames 2000
.\gb\build\gbemu.exe --rom roms\blargg\mem_timing.gb --serial - --frames 2000
```

**Gate:** `instr_timing`, `mem_timing` and `mem_timing-2` all print `Passed`, and
nothing from M01-M08 regressed. This is the milestone where regressions are
normal; bisect with the tracer instead of guessing. Commit.

---

## M10 — APU

Tests: you write the five `m10_*` tests

- [ ] Read `docs/05-audio.md` end to end
- [ ] `NR52` power, `NR50` master volume, `NR51` panning, and the DAC concept
- [ ] CH1/CH2 square: duty patterns, frequency `131072 / (2048 - X)`
- [ ] CH3 wave RAM; CH4 noise LFSR
- [ ] Frame sequencer: 512 Hz, 8 steps, length at 256 Hz, envelope/sweep on the right steps
- [ ] Downsample per T-cycle accumulation to 48 kHz and dump a WAV

```
.\gb\build.cmd -Test m10_
.\gb\build\gbemu.exe --rom roms\blargg\dmg_sound\01-registers.gb --serial - --frames 4000
```

**Gate:** `dmg_sound` sub-tests 01-06 report pass, and a real game's music is
recognisable (not just noise). Commit.

---

## M11 — Save states, disassembler, debugger

Tests: you write `m11_savestate_roundtrip`, `m11_disassembler_covers_all_ops`,
`m11_trace_ring_keeps_last_n`

- [ ] Read `docs/06` §6.6
- [ ] `savestate.h`/`savestate.c`: serialise everything in `gb_t` including cart RAM and RTC
- [ ] Real `gb_disasm` on your opcode table; it must decode all 256 + 256 CB entries
- [ ] Debugger: single step, register/memory inspection, breakpoints on PC and on memory access, `--load-state`
- [ ] SDL2 window (`pacman -S mingw-w64-ucrt-x86_64-SDL2`) or keep the BMP/ffmpeg pipeline; add input mapping and fast-forward

**Gate:** a save state round trip is byte-identical, the disassembler covers every
opcode, and you can play a game interactively. Commit.

---

## M12 — Stretch (pick one)

- [ ] **CGB:** header flag, `KEY1`/double speed, `FF4F` VRAM bank, `FF68-FF6B` palette RAM, per-tile attribute byte; tests `m12_cgb_palettes`, `m12_double_speed`
- [ ] **Python differential fuzzer:** random register states and random opcode streams through both the C core and a Python model, reporting the first diverging field; see `tools/`
- [ ] Or your own project with a written definition of done

---

## Stuck?

1. Read the failure out loud: `UNIMPLEMENTED at file:line` = type code there;
   `expected X, got Y` = a real bug with a number in it.
2. `docs/06-verification-and-tooling.md` §6.7 maps symptom -> likely cause. Use it
   before you change anything.
3. Trace. `--trace build\trace.txt --frames 1`, then read the last 20 lines and
   find the **first** instruction whose result is impossible. Everything after it
   is noise.
4. If you cannot form a hypothesis in 45 minutes, stop editing and read the spec
   section again. Write the hypothesis down before touching the keyboard.
5. Rule of thumb: 90% of emulator bugs are in the 10% of the spec you skimmed.
   `H`/`C` flags, the window line counter, sprite priority, `DAA`'s `0x60`.

## Session log

Keep a running block like this in `NOTES.md`:

```
M03  2024-05-04  9 failed / 1 passed -> first: m03_alu_add_flags: expected 0x20, got 0x00
                 cause: half-carry computed as unsigned carry. fixed. 10/10.
M03  2024-05-04  regression m0: m01_bus_wram_echo broke (echo off-by-one). fixed.
```
