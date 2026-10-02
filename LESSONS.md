# LESSONS — the 90-minute lesson ladder

This file replaces the milestone-as-lesson model. Read it once, then work the
lessons in order.

## The rules of this structure

1. **No lesson is longer than 90 minutes.** Each one states its own budget:
   theory, coding, verification. If a lesson is running long, you have hit a bug,
   not a long lesson — stop and use §4 of that lesson.
2. **A lesson is self-contained.** All the theory you need is *inside* it. The
   `docs/` folder is optional deeper reading, never a prerequisite.
3. **Every lesson names the tests that prove it.** The tests are already written
   and already failing. You implement until they are green. You never write the
   test for a lesson that provides one.
4. **Core and AHA first.** You get something running, visibly wrong, or visibly
   right as early as possible. Precision work — timing, rare opcodes, MBC
   variants, audio — comes after the machine works.
5. **A phase gate is what makes it true.** Phase gates are external ROMs
   (`cpu_instrs`, `dmg-acid2`, `instr_timing`, `dmg_sound`); they are listed at the
   end of each phase and they override your own unit tests.

The length of the whole ladder is ~34 lessons ≈ 50 hours. That is what a DMG
emulator that boots real games actually costs.

## The AHA ladder

You should feel five distinct "oh — it works" moments:

| AHA | Where | What you will see |
| --- | --- | --- |
| 1. The loop works | L01 | a red suite that is a to-do list, not a disaster |
| 2. I am reading real hardware data | L02, L03 | a real cartridge's title, size and MBC printed by your own header parser |
| 3. I drew a picture | L04 | a BMP on your disk containing pixels you made, with no CPU involved |
| 4. My CPU is correct | L18 | blargg `cpu_instrs` printing `Passed` |
| 5. A real ROM runs on my machine | L20 (32 KiB ROM), L26 (a commercial game, after MBC1) | a title screen, then a pixel-perfect `dmg-acid2` at L24 |

## Phase 1 — See it work (L01-L20, ~30 h)

Theory-light, result-heavy. Every lesson ends with either green tests or a file
you can look at.

| # | Lesson | Min | You build | Tests that prove it |
| --- | --- | --- | --- | --- |
| [L01](lessons/L01-the-feedback-loop.md) | The feedback loop | 90 | build, harness, tracer, the one-struct design | `m00_smoke_builds`, `m00_types_and_bits`, `m00_harness_selfcheck` |
| [L02](lessons/L02-read-a-real-cartridge.md) | Read a real cartridge | 90 | `cart_load`: header, checksum, ROM copy, ROM-only reads | `m01_header_parse`, `m01_checksum`, `m01_cart_read_rom` |
| [L03](lessons/L03-the-address-decoder.md) | The address decoder | 90 | `bus_read`/`bus_write`: the whole map, echo RAM, unmapped, 16-bit | `m01_bus_wram_echo`, `m01_bus_unmapped_ff`, `m01_bus_hram_ie` |
| [L04](lessons/L04-first-pixels.md) | **First pixels** | 90 | the PPU framebuffer, palettes, BG tile decode — from hand-written VRAM, no CPU | `m07_lcd_off_blank`, `m07_bg_tile_decode` |
| [L05](lessons/L05-the-cpu-wakes-up.md) | The CPU wakes up | 90 | registers, `fetch8`/`fetch16`, PC, NOP, `LD r,d8`, cycles | `m02_nop_advances_pc`, `m02_step_cycles_nop`, `m02_ld_r_d8` |
| [L06](lessons/L06-loads-jumps-and-loud-failure.md) | Loads, jumps, and loud failure | 90 | `LD rr,d16`, `JP a16`, the 11 illegal opcodes fault | `m02_ld_rr_d16`, `m02_pc_after_2byte`, `m02_illegal_opcode_traps` |
| [L07](lessons/L07-the-load-matrix.md) | The load matrix | 90 | `LD r,r'`: 64 opcodes from one rule, plus the `(HL)` operand | `m03_ld_r_r_matrix` |
| [L08](lessons/L08-addition-and-carry.md) | Addition and carry | 90 | `ADD`/`ADC`, the `H` and `C` rules, register and immediate forms | `m03_alu_add_flags`, `m03_alu_adc_flags` |
| [L09](lessons/L09-subtraction-and-compare.md) | Subtraction and compare | 90 | `SUB`/`SBC`/`CP`: borrows, carry-in, and why `CP` writes nothing | `m03_alu_sub_flags`, `m03_alu_sbc_flags`, `m03_alu_cp_no_write` |
| [L10](lessons/L10-logic-counting-and-the-hl-cost.md) | Logic, counting, and the `(HL)` cost | 90 | `AND`/`OR`/`XOR` (yes, `AND` sets `H`), `INC`/`DEC` keeping `C` | `m03_alu_logic_flags`, `m03_inc_dec_flags`, `m03_hl_indirect_cycles` |
| [L11](lessons/L11-rotates-and-bcd.md) | Rotates and BCD | 90 | `RLCA` family (Z always 0), `CPL`/`SCF`/`CCF`, `DAA` | `m03_rotates_a`, `m03_daa` |
| [L12](lessons/L12-jumps-and-your-first-program.md) | Jumps and your first program | 90 | `JP`/`JR` with conditions, relative offsets, then run a real program | `m04_jp_jr_conditions`, `m04_program_loop_sum` |
| [L13](lessons/L13-the-stack.md) | The stack | 90 | `PUSH`/`POP` byte order, `CALL`/`RET`/`RETI`/`RST` | `m04_call_ret_stack`, `m04_push_pop_order`, `m04_rst_vectors` |
| [L14](lessons/L14-the-cb-page.md) | The CB page | 90 | `BIT`/`RES`/`SET` and the eight shifts, from two rules | `m04_cb_rotates`, `m04_cb_bit_res_set` |
| [L15](lessons/L15-interrupts-and-halt.md) | Interrupts and HALT | 90 | `IF`/`IE`/`IME`, dispatch, the `EI` delay, `HALT` | `m05_interrupt_dispatch`, `m05_ime_ei_delay`, `m05_halt_wake`, `m05_halt_bug` |
| [L16](lessons/L16-the-timer.md) | The timer | 90 | `DIV` from one counter, `TIMA` from a divider bit, overflow reload | `m05_timer_div_rate`, `m05_timer_tima_overflow`, `m05_timer_tma_reload` |
| [L17](lessons/L17-the-last-opcodes-and-serial.md) | The last opcodes, and serial | 90 | `LDH`, the `(C)` forms, SP arithmetic, `JP HL`, and the port that prints | `m05_serial_emits_byte` — every base opcode except `STOP` |
| [L18](lessons/L18-the-clock-vblank-and-the-boot-state.md) | The clock, VBlank, and the boot state | 90 | one clock in `bus_tick`, the VBlank interrupt, the post-boot registers | `m06_boot_without_bootrom`, `m06_frame_is_70224`, `m06_vblank_interrupt_fires` **+ gate: `cpu_instrs` says `Passed`** |
| [L19](lessons/L19-the-joypad.md) | The joypad | 90 | the `FF00` matrix and its edge-triggered interrupt | `m06_joypad_matrix` |
| [L20](lessons/L20-a-real-rom-runs.md) | **A real ROM runs** | 90 | run a 32 KiB ROM for 600 frames, dump a frame, read a trace | `m06_run_60_frames` + your own eyes |

## Phase 2 — Make it look right (L21-L25, ~7 h)

| # | Lesson | Min | You build | Proof |
| --- | --- | --- | --- | --- |
| [L21](lessons/L21-sprites.md) | Sprites | 90 | OAM, `+16`/`+8` offsets, 8x16, flips, palettes, the 10-per-line limit, DMG priority | provided `m07_sprite_priority_x`, `m07_sprite_10_per_line` |
| [L22](lessons/L22-the-window-and-scrolling.md) | The window and scrolling | 90 | `WX-7`/`WY`, the window's own line counter, 256x256 BG wrap | provided `m07_window_position`, `m07_bg_scroll_wrap` |
| [L23](lessons/L23-stat-and-lyc.md) | STAT and LYC | 90 | mode interrupts, the LYC coincidence, rising-edge semantics | provided `m07_stat_modes_timing`, `m07_lyc_coincidence` |
| [L24](lessons/L24-dmg-acid2.md) | **dmg-acid2** | 90 | fix what the reference face tells you is wrong | gate: the correct face, no stray pixels |
| [L25](lessons/L25-raster-effects.md) | Raster effects | 90 | per-line register sampling: status bars and split screens | provided `m07_raster_scroll`, plus a game with a status bar |

## Phase 3 — Make games work (L26-L31, ~9 h)

The labourious part. Correctness, not features. Every lesson here is
unit-test-driven for the same reason: this is where regressions happen.

| # | Lesson | Min | You build | Proof |
| --- | --- | --- | --- | --- |
| [L26](lessons/L26-mbc1-banking.md) | MBC1: banking | 90 | the snooped-write model, bank 0 -> 1, `ram_enabled`, mode select | provided `m08_mbc1_bank0_quirk`, `m08_mbc1_mode_ram` |
| [L27](lessons/L27-save-ram.md) | Save RAM | 90 | `<rom>.sav`, load on open, save on exit | provided `m08_battery_save_roundtrip` |
| [L28](lessons/L28-mbc3-mbc5-and-the-rtc.md) | MBC3, MBC5 and the RTC | 90 | 7-bit banks, 9-bit banks, the latch sequence | provided `m08_mbc3_rtc`, `m08_mbc5_9bit` |
| [L29](lessons/L29-access-level-timing.md) | Access-level timing | 90 | tick the bus per access, `ADD SP,e8` internal cycles | provided `m09_access_cycle_costs` + gate: `instr_timing`, `mem_timing` |
| [L30](lessons/L30-the-awkward-quirks.md) | The awkward quirks | 90 | OAM DMA stall, VRAM/OAM access windows, STAT blocking, the TIMA reload delay, the DIV phase reset | provided `m09_timer_overflow_delay`, `m09_div_apu_edge`, `m09_oam_dma_timing`, `m09_stat_blocking` |
| [L31](lessons/L31-the-halt-bug-and-the-long-tail.md) | The halt bug and the long tail | 90 | the double-fetch, `STOP`, the DIV write glitch | enabled `m05_halt_bug` variant + `halt_bug.gb` |

## Phase 4 — Nice to have (L32-L36, ~8 h)

Skippable in any order, and skippable entirely.

| # | Lesson | Min | You build | Proof |
| --- | --- | --- | --- | --- |
| [L32](lessons/L32-save-states.md) | Save states | 90 | serialise `gb_t` including cart RAM | provided `m11_savestate_roundtrip` |
| [L33](lessons/L33-the-disassembler-and-the-debugger.md) | Disassembler and debugger | 90 | real `gb_disasm`, breakpoints, single step | provided `m11_disassembler_covers_all_ops`, `m11_trace_ring_keeps_last_n` |
| [L34](lessons/L34-a-real-window.md) | A real window | 90 | SDL2, scaling, keyboard map, fast-forward | a game you can play |
| [L35](lessons/L35-the-apu-square-channels.md) | Sound: the square channels | 90 | `NR52` power, mixing, CH1/CH2, the frame sequencer | provided `m10_ch1_square`, `m10_frame_sequencer_512hz` |
| [L36](lessons/L36-wave-noise-and-the-stretch.md) | Sound: wave, noise, envelopes | 90 | CH3/CH4, envelopes, length, sweep | provided `m10_wave_channel`, `m10_noise_lfsr`, `m10_volume_envelope`; gate: `dmg_sound` 01-06; optional stretch `m12_cgb_palettes`, `m12_double_speed` |

Stretch, not on the ladder: CGB support (`m12_*`), a Python differential fuzzer.

## The lesson template

Every lesson in `lessons/` has exactly these sections. Use it as your
checklist when you think a lesson is "missing something":

```
# LNN — Title
**Time**       theory ~25 min | coding ~55 min | verify ~10 min
**You will end with**  the observable result (green tests, a file, a number)
**Tests that must go green**  exact names, plus the command
**Depends on**  earlier lessons and the files you will touch

## 1. Read this (the whole theory for today)   [25 min]
   self-contained. Tables, rules, the layout. No "see docs/02".
   > **What matters for the code you are about to write**
   > the one or two rules/paragraphs that the implementation depends on, quoted
   > in the same place as the rest so you cannot miss the connection.

## 2. Your task   [55 min]
   numbered steps. Each names the file, the function and the command.

## 3. Prove it   [10 min]
   the command, the expected output, and what "green" looks like.

## 4. If it fails
   symptom -> cause -> how to find it. The 4-5 most likely, in order.

## 5. Done when
   a short checklist, including "and you can explain X out loud".

## 6. Optional, only if you have time
   deeper reading in docs/, and one small stretch.
```

## What is deferred, and why that is deliberate

| Deferred | To | Why |
| --- | --- | --- |
| `DAA` corner cases beyond the table in L11 | L31 | rare in real games, needed only for `cpu_instrs` perfection |
| MBC2, MBC3 RTC, MBC5 rumble | L28 | most games you will test use ROM-only or MBC1 |
| Cycle-exact memory timing | L29 | nothing observable until you chase `mem_timing` |
| OAM DMA stall, VRAM access windows, STAT blocking | L30 | needed by a handful of games and two test ROMs |
| The halt bug's double fetch | L31 | one test ROM |
| Audio | L35-L36 | the largest subsystem with the worst testability |
| Save states, debugger, window | L32-L34 | they make you faster, they do not make the emulator more correct |

## Relationship to the milestone briefs

`milestones/` still holds the M00-M12 briefs. They are **planning documents**: per
milestone you get the deliverable contract, the full work order, the trap list and
a three-tier hint ladder. A milestone is 3-6 lessons. Use them when you want to see
where a lesson sits in the larger arc, or when you are stuck and want the hint
ladder (which the lessons deliberately do not contain — a self-contained lesson
tells you the rule, not the workaround).

**The lessons are the work. The milestone briefs are the map.**

## Current authoring status

| Phase | Lessons | Status |
| --- | --- | --- |
| 1 | L01-L04 | **authored** — feedback loop, cartridge, bus, and L04's three provided PPU tests |
| 1 | L05-L08 | **authored** — the CPU core: fetch/decode, loads, the `LD` matrix, `ADD`/`ADC` |
| 1 | L09-L12 | **authored** — the rest of the ALU, rotates, `DAA`, and the first running program |
| 1 | L13-L16 | **authored** — the stack, the whole CB page, interrupts/HALT, the timer |
| 1 | L17-L20 | **authored** — the last opcodes + serial, the clock and `cpu_instrs` gate, joypad, first ROM |
| 2 | L21-L22 | **authored** — sprites, and the window with scrolling; their four tests are now real |
| 2 | L23 | **authored** — STAT modes, the LYC coincidence and the STAT interrupt |
| 2 | L24-L25 | **authored** — the `dmg-acid2` gate, and raster effects with a provided test |
| 3 | L26-L28 | **authored** — MBC1, battery saves, and MBC3/MBC5 with the RTC latch |
| 3 | L29-L31 | **authored** — access-level timing, the four awkward quirks, and the halt bug |
| 4 | L32-L34 | **authored** — save states, the disassembler and debugger, and the SDL2 window |
| 4 | L35-L36 | **authored** — the APU (square, wave, noise, envelopes) and the CGB stretch |

Test inventory as of this writing: **77 real tests, 0 `TEST_TODO` stubs**. Every test the
ladder promises exists and is registered, covering the toolchain, the CPU and flags,
interrupts and the halt bug, the timer, serial, the frame loop, the joypad, the PPU, MBC
banking and saves, access-level timing, save states, the disassembler, the tracer, the four
APU channels, and the two CGB stretch goals. 77 registered in total.

Of those, 75 are the DMG course: green when the emulator is finished. The two `m12_*` tests
are the optional CGB stretch and stay red until you choose to take it — a goal you pick up,
not a debt you inherited.

A note on which lesson a test belongs to: a test is owned by the **last** lesson it
depends on, not the first. `m03_hl_indirect_cycles` is the clearest case — it needs
`LD (HL),r` (L07), `ADD A,(HL)` (L08) and `INC (HL)` (L10), so it is L10's proof.

Every lesson is authored: all 36 are written, and each one had its tests registered before it
was written. Nothing here is a placeholder.
