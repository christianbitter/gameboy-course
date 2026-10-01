# Game Boy (DMG) Emulator — External Reference Index

Scope: original monochrome Game Boy (DMG). Everything below was checked by fetching the
page/file in question. Where a source could not be fetched, that is stated explicitly.

**Fetching note (read first).** `gbdev.io` and `rgbds.gbdev.io` return **HTTP 403** to this
environment's HTTP client for every path, including the site root. These URLs are still the
canonical, correct URLs (they are the links the Pan Docs source and the awesome-gbdev list
themselves use), but their *content* below was read from the same documents' sources in the
`gbdev/pandocs` and `gbdev/gb-opcodes` repositories on `raw.githubusercontent.com`, which serve
byte-identical Markdown/JSON. `gekkio.fi`, all `github.com` and `raw.githubusercontent.com`
URLs, `sameboy.github.io`, `bgb.bircd.org`, `emulicious.net`, `devrs.com`, `gbdev.gg8.se` and
`discord.gg` all returned HTTP 200.

---

## 1. Canonical hardware documentation

### Pan Docs (the community's single most comprehensive GB reference)

| URL | One-line annotation |
|---|---|
| <https://gbdev.io/pandocs/> | Entry point; the whole reference; "corrected, updated and maintained by the community" (403 to this client — see note above). |
| <https://gbdev.io/pandocs/Power_Up_Sequence.html> | Boot ROM behaviour **and the post-hand-off CPU register and I/O register tables** — the single source for "what state is the machine in at $0100". |
| <https://gbdev.io/pandocs/CPU_Instruction_Set.html> | How SM83 bytes decode into instructions; bit-field breakdown of every block; **the list of 11 invalid opcodes that hard-lock the CPU**. |
| <https://gbdev.io/pandocs/CPU_Registers_and_Flags.html> | The A/F/B/C/D/E/H/L registers, the flag byte layout, and the fact that F's low nibble is unimplemented. |
| <https://gbdev.io/pandocs/Interrupts.html> | `IME` semantics (write-only, `ei`/`di`/`reti`, delayed `ei`, cleared when a handler runs), `IF` ($FF0F), `IE` ($FFFF), priorities, and the 5 M-cycle ISR entry sequence. |
| <https://gbdev.io/pandocs/Interrupt_Sources.html> | What actually requests each interrupt (VBlank, STAT, Timer, Serial, Joypad) and the rising-edge nature of the STAT interrupt line. |
| <https://gbdev.io/pandocs/Timer_and_Divider_Registers.html> | `DIV`/`TIMA`/`TMA`/`TAC` ($FF04–$FF07), the TAC clock-select table in M-cycles, and the TMA-on-overflow race. |
| <https://gbdev.io/pandocs/Timer_Obscure_Behaviour.html> | The system counter, the **DIV-write / TAC-write "timer tick" glitches**, and the fact that TIMA→TMA reload + IF happens **one M-cycle late**. |
| <https://gbdev.io/pandocs/Rendering.html> | Frame = 154 scanlines; the four PPU modes with dot counts (mode 2 = 80 dots, mode 3 = 172–289 dots, mode 1 = 4560 dots); mode-3 "penalties" from SCX, window and objects. |
| <https://gbdev.io/pandocs/STAT.html> | `STAT` ($FF41) bit layout, mode bits reading 0 when the PPU is off, and the **spurious STAT interrupt** quirk on monochrome hardware. |
| <https://gbdev.io/pandocs/LCDC.html> | `LCDC` ($FF40) bit meanings, the "turn the LCD off only during VBlank or you damage the panel" warning, and mid-frame/mid-scanline behaviour. |
| <https://gbdev.io/pandocs/Accessing_VRAM_and_OAM.html> | VRAM is CPU-accessible only in modes 0–2 and OAM only in modes 0–1; blocked writes are dropped and blocked reads return garbage. |
| <https://gbdev.io/pandocs/OAM.html> | OAM layout, the 10-objects-per-scanline limit, "hidden" objects still counting toward the limit, and DMG vs CGB object priority. |
| <https://gbdev.io/pandocs/OAM_DMA_Transfer.html> | `DMA` ($FF46), the 160 M-cycle / 640-dot transfer, and the **DMG bus conflict that restricts the CPU to HRAM during OAM DMA**. |
| <https://gbdev.io/pandocs/OAM_Corruption_Bug.html> | The DMG-only OAM corruption caused by 16-bit inc/dec of a value in $FE00–$FEFF during the first ~20 cycles of a visible scanline. |
| <https://gbdev.io/pandocs/Memory_Map.html> | The full address map, Echo RAM mirroring, the FEA0–FEFF "prohibited" range, and the jump-vector/interrupt-vector layout. |
| <https://gbdev.io/pandocs/Audio.html> | APU overview plus the classic `NR52`-power / `NR51`-panning / wave-RAM traps. |
| <https://gbdev.io/pandocs/Audio_Registers.html> | `NR10`–`NR52` and wave RAM, register by register, including read/write masks on DMG. |
| <https://gbdev.io/pandocs/Palettes.html> | `BGP`/`OBP0`/`OBP1` ($FF47–$FF49) shade mapping and the CGB palette registers. |
| <https://gbdev.io/pandocs/halt.html> | `halt` semantics and the **HALT bug** (IME=0 + pending interrupt ⇒ PC not incremented). |
| <https://gbdev.io/pandocs/Hardware_Reg_List.html> | Flat table of every I/O register with its address, so you can sanity-check your `$FF00`–`$FF7F` decode. |
| <https://gbdev.io/pandocs/The_Cartridge_Header.html> | Header layout and checksums — needed to accept real ROMs and to build your own test ROMs. |
| <https://gbdev.io/pandocs/CPU_Comparison_with_Z80.html> | Exactly where SM83 diverges from the Z80; prevents copying Z80 cycle counts or flags. |

### "Game Boy: Complete Technical Reference" (gbctr) by Joonas "gekkio" Javanainen

| URL | One-line annotation |
|---|---|
| <https://gekkio.fi/files/gb-docs/gbctr.pdf> | The canonical PDF (176 pages, fetched successfully). Per-instruction **M-cycle-by-M-cycle timing diagrams**, opcode tables, boot ROM hashes and hardware details that Pan Docs refers to rather than duplicating. |
| <https://github.com/Gekkio/gb-ctr> | Source repo (Typst). License file present. Using this file is how you can audit or cite a specific claim. |
| <https://raw.githubusercontent.com/Gekkio/gb-ctr/main/opcodes.toml> | Machine-readable opcode table: 256 entries + 256 CB entries with mnemonic and category; **the undefined base opcodes are exactly the entries whose `category = "undefined"`**. |
| <https://raw.githubusercontent.com/Gekkio/gb-ctr/main/chapter/cpu/instruction-set.typ> | The prose+timing source behind the PDF; each conditional instruction carries explicit `cc_true` / `cc_false` durations. |
| <https://raw.githubusercontent.com/Gekkio/gb-ctr/main/appendix/opcode-tables.typ> | The instruction-set and CB-prefixed opcode table generators, with the "undefined" colour legend. |

### Opcode tables (the tables Pan Docs links to)

| URL | One-line annotation |
|---|---|
| <https://gbdev.io/gb-opcodes/optables/> | Pan Docs' own recommended opcode/flag cheat sheet (403 to this client). |
| <https://gbdev.io/gb-opcodes/optables/octal> | Same data in octal, which makes the encoding patterns obvious. |
| <https://raw.githubusercontent.com/gbdev/gb-opcodes/master/Opcodes.json> | The machine-readable source behind those tables: per opcode `mnemonic`, `bytes`, `cycles` (a 1- or 2-element T-cycle array), operands and flag effects. **CC0-1.0** (public domain) — safe to vendor into your repo. |
| <https://rgbds.gbdev.io/docs/gbz80.7> | RGBDS's `gbz80(7)`: every instruction with byte length, cycle count and flag effects (403 to this client; linked from Pan Docs' CPU instruction page). |

### gbdev.io / other canonical entry points

| URL | One-line annotation |
|---|---|
| <https://gbdev.io/> | The community hub (403 to this client). Also reachable as `gbdev.github.io`. |
| <https://gbdev.io/gb-asm-tutorial> | "Game Boy assembly tutorial" — a real, staged tutorial from empty ROM to working game (403 to this client). |
| <https://github.com/gbdev/rgbds> | RGBDS assembler/linker; also the best-documented assembler for writing your own test ROMs. |
| <https://gbhwdb.gekkio.fi/> | gekkio's Game Boy hardware database: per-console mainboard/SoC revisions. Use it to see why a test has `-dmg0` / `-dmgABC` variants. |

### Vendored machine-readable tables (local copies, for offline use)

| Path | One-line annotation |
|---|---|
| `tables/gb-opcodes-Opcodes.json` | Verbatim copy of the CC0 `gbdev/gb-opcodes` `Opcodes.json`: all 256 base + 256 CB opcodes with `bytes`, `cycles` (T-cycles; a 2-element array means `[taken, not taken]`) and flag effects. |
| `tables/gbctr-opcodes.toml` | Verbatim copy of `Gekkio/gb-ctr`'s `opcodes.toml`: the same opcode space classified by category, with the 11 undefined base opcodes marked `category = "undefined"`. |

### Official Sharp/Nintendo primary material

The original Sharp SM83 CPU datasheet and the DMG schematics are not freely redistributable and
were **not** verified here. Use gbctr (which is derived from decaps and datasheets) instead.

---

## 2. How-to-emulate / tutorial-quality articles

There is very little *emulator-writing* tutorial material that is both good and primary.
Verified candidates, in rough order of usefulness:

| URL | One-line annotation |
|---|---|
| <https://gbdev.io/gb-asm-tutorial> | Not emulator-specific, but the best free course on what GB programs actually do — it makes the hardware model concrete (403 to this client). |
| <https://github.com/Gekkio/mooneye-gb/blob/master/README.markdown> | Doubles as a design note: gekkio states his accuracy goals and links gbctr for "why". Useful as a model for how to structure an emulator's correctness argument. Verified: fetched from `raw.githubusercontent.com`. |
| <https://gbdev.io/pandocs/Rendering.html> | The closest thing to a "how the PPU works" walkthrough; read it before writing a renderer. |
| <https://github.com/gbdev/awesome-gbdev> | The curated index. Its "Emulator Development" and "Tools" sections are the practical starting list. |
| <https://gbdev.io/guides/dma_hijacking> | GB-specific technique article; useful background for why DMA timing matters (403 to this client). |

**Deliberately not recommended:** the many "build a Game Boy emulator in 1 hour" blog posts.
They were not verified and none of them is a primary source. Use Pan Docs + gbctr instead.

---

## 3. CPU test ROM suites — blargg's gb-test-roms

Repo: <https://github.com/retrio/gb-test-roms> (the de-facto mirror; blargg's ROMs are free to
use, and the repo's root `readme.txt` points at the original/current official hosting at
`blargg.8bitalley.com`). **License: no explicit license file — UNVERIFIED.** They are
universally redistributed and used for exactly this purpose, but do not assume a specific SPDX id.

Each test prints progress and a final result on screen **and** over the link port (writing the
character to `SB`, then `$81` to `SC`), so an emulator can read results from serial output
instead of OCR'ing the screen. "Everything printed on screen is also sent to the game link port"
is stated in every `readme.txt`. The `oam_bug` suite additionally writes results to cart RAM at
`$A000`, with signature `$DE $B0 $61` at `$A001-$A003` and a NUL-terminated text string from
`$A004` — the easiest machine-readable channel.

| Directory / ROM | What it covers |
|---|---|
| `cpu_instrs/` (`cpu_instrs.gb`) | "tests the behavior of all CPU instructions **except STOP and the 11 illegal opcodes**", with boundary data; verifies results **and that other registers are not modified**. Sub-tests: `01-special`, `02-interrupts`, `03-op sp,hl`, `04-op r,imm`, `05-op rp`, `06-ld r,r`, `07-jr,jp,call,ret,rst`, `08-misc instrs`, `09-op r,r`, `10-bit ops`, `11-op a,(hl)`. |
| `instr_timing/` (`instr_timing.gb`) | T-cycles of every instruction except HALT/STOP/illegal. **"For conditional instructions, it tests taken and not taken timings."** Requires a working timer (TAC/TIMA/TMA). The readme embeds blargg's verified cycle table — the single best cross-check for §B of the report. |
| `mem_timing/` and `mem_timing-2/` | Which T-cycle each instruction performs its memory read/write on (`01-read_timing`, `02-write_timing`, `03-modify_timing`). `-2` is the revised suite. Requires correct instruction timing and timer operation. |
| `oam_bug/` | The DMG OAM-corruption bug: 16-bit inc/dec of a value in `$FE00-$FEFF` during the first ~20 cycles of a visible scanline. 8 singles: `1-lcd_sync`, `2-causes`, `3-non_causes`, `4-scanline_timing`, `5-timing_bug`, `6-timing_no_bug`, `7-timing_effect`, `8-instr_effect`. |
| `dmg_sound/` | DMG APU behaviour; 12 singles covering registers, length counter, trigger, sweep, sweep details, overflow-on-trigger, length/sweep period sync, length counter during power, wave read while on, wave trigger while on, registers after power, wave write while on. |
| `cgb_sound/` | The same for CGB's APU. **Not DMG-relevant.** |
| `halt_bug.gb` | The HALT bug in isolation. |
| `interrupt_time/` (`interrupt_time.gb`) | Interrupt timing. |

Notes the repo's own readmes give: the multi-ROM builds "print a test's number, run the test,
then 'ok' if it passes, otherwise a failure code"; **"once a sub-test fails, no further tests for
that file are run"**; and **"Currently there is no well-defined way for an emulator test rig to
programatically find the result of the test"** — hence screenshot comparison or the serial
output. Failure code 1 is a generic failure; other codes are found by searching for `set_test n`
in the matching file under `source/`.

---

## 4. Whole-system test ROM suites

### 4a. Mooneye Test Suite (gekkio) — the DMG accuracy gold standard

* Repo: <https://github.com/Gekkio/mooneye-test-suite>
* Prebuilt ROMs: <https://gekkio.fi/files/mooneye-test-suite/> (rebuilt and redeployed on every
  change to `main`).
* License: **MIT**, "Copyright (C) 2014-2022 Joonas Javanainen" (read from the repo's `LICENSE`).

Suite structure (from the README): `acceptance` (the main suite, hardware-verifiable),
`emulator-only`, `madness`, `manual-only`, `misc` (CGB/AGB extras), `utils`.
Test name suffixes select the console model: `dmg`, `mgb`, `sgb`, `sgb2`, `cgb`, `agb`, `ags`,
and group letters `G` = dmg+mgb, `S` = sgb+sgb2, `C` = cgb+agb+ags, `A` = agb+ags. DMG SoC
revisions are `DMG 0, A, B, C` — a `-dmg0` test may legitimately fail on your `dmgABC` target.

**How it reports pass/fail (exact protocol, from the README):**
* *Pass:* writes the Fibonacci numbers 3/5/8/13/21/34 to **B/C/D/E/H/L**, executes an **`LD B, B`**
  opcode (used as a "debug breakpoint" by some emulators), sends the same numbers over the **link
  port** (busy-waiting, so the serial interrupt need not be implemented), then executes another
  `LD B, B` followed by an infinite `JR` to itself.
* *Fail:* writes **`0x42`** to **B/C/D/E/H/L**, executes `LD B, B`, sends `0x42` six times over
  serial, then `LD B, B` and an infinite `JR` self-loop.
* Speed-up trick stated in the README: make `LY` ($FF44) and `SC` ($FF02) **both return $FF** when
  read, which bypasses drawing and the serial-completion wait.

Highest-value DMG tests for a beginner emulator: `acceptance/`: `boot_regs-dmgABC`,
`boot_hwio-dmgABCmgb`, `boot_div-dmgABCmgb`, `add_sp_e_timing`, `ld_hl_sp_e_timing`,
`call_cc_timing`, `jp_cc_timing`, `ret_cc_timing`, `push_timing`, `pop_timing`, `rst_timing`,
`instr/daa`, `bits/reg_f`, `bits/mem_oam`, `interrupts/ie_push`, `ei_sequence`, `ei_timing`,
`halt_ime0_ei`, `halt_ime1_timing`, `intr_timing`, `div_timing`, `rapid_toggle`, `tim00`–`tim11`
plus their `-div_trigger` variants, `tima_reload`, `tima_write_reloading`, `tma_write_reloading`,
`oam_dma/basic`, `oam_dma/reg_read`, `oam_dma_start`, `oam_dma_restart`, `oam_dma_timing`,
`ppu/intr_2_0_timing`, `ppu/intr_2_mode0_timing`, `ppu/intr_2_mode3_timing`,
`ppu/intr_2_oam_ok_timing`, `ppu/stat_irq_blocking`, `ppu/stat_lyc_onoff`.

`mooneye-gb` itself (the emulator) is at <https://github.com/Gekkio/mooneye-gb>, **GPL-3.0**.
It is doubly useful here because its README contains a per-test pass/fail table for a known-good
implementation — including honest `:x:` entries.

### 4b. dmg-acid2 (Matt Currie) — the PPU conformance image

* Repo: <https://github.com/mattcurrie/dmg-acid2>
* ROM: <https://github.com/mattcurrie/dmg-acid2/releases/download/v1.0/dmg-acid2.gb>
* License: **MIT**, "Copyright (c) 2020 Matt Currie".
* **How it reports:** visually. Your rendered frame must be **pixel-identical** to
  `img/reference-dmg.png`. For DMG greyscale output use exactly `$00`, `$55`, `$AA`, `$FF` as the
  four shade values; for a CGB running in DMG mode use `(r << 3) | (r >> 2)` per 5-bit component.
  It is *not* a T-cycle PPU torture test and does not write registers during mode 3 — a simple
  line-based renderer should pass. The README documents each region (mohawk, eyes, moles, mouth,
  chin, footer) and the exact PPU feature it exercises, plus a table of failure images.
* CGB counterpart: <https://github.com/mattcurrie/cgb-acid2> (also MIT).
* Harder PPU-timing follow-up: <https://github.com/mattcurrie/mealybug-tearoom-tests>.

### 4c. SameBoy tests (Lior Halphon)

* Emulator repo: <https://github.com/LIJI32/SameBoy>
* Releases: <https://github.com/LIJI32/SameBoy/releases> (prebuilt macOS/Windows/Linux binaries,
  including the CLI tester).
* License: **Expat/MIT** for everything except the `iOS` and `HexFiend` directories (read from the
  repo's `LICENSE`). Note GitHub reports it as `NOASSERTION`, so read the file, not the API.
* Automation results: <https://sameboy.github.io/automation/>.
* **How it reports:** the in-repo `Tester/` harness runs a ROM for N frames, dumps a BMP, and the
  CI script `.github/actions/sanity_tests.sh` **compares the BMP's SHA-1 against a hard-coded
  expected hash**, exiting non-zero and printing the failing `.bmp` names on mismatch. It runs
  `cgb_sound.gb`, `cgb-acid2.gbc`, `dmg-acid2.gb` (twice, in DMG and CGB-DMG modes),
  `dmg_sound-2.gb` and `oam_bug-2.gb`.
* Its README also records that SameBoy passes all of blargg's, all of mooneye-gb's and all of
  Wilbert Pol's tests.

### 4d. Gambatte tests

* Upstream (with the test harness): <https://github.com/pokemon-speedrunning/gambatte-core> —
  **GPL-2.0**. Test tree: `test/hwtests/` (per-feature `.asm` sources with reference PNGs named
  `<test>_<model>.png`, e.g. `_dmg08`, `_cgb04c`), plus `test/testrunner.cpp`,
  `test/qdgbas.py`, `test/scripts/assemble_tests.sh`, `test/scripts/run_tests.sh`.
* The better-known packaging used by emulator authors is the LibreIP/libretro fork:
  <https://github.com/libretro/gambatte-libretro> — **GPL-2.0**.
* **How it reports:** the tests are assembled with `qdgbas`, run through `testrunner`, and the
  resulting frame is **diffed against a per-console-model reference PNG**; a mismatch is a failure.
  This is image comparison, not an on-screen message. (The README's emphasis: "The development of
  numerous tests, and their verification on hardware, is as much an emphasis as the development of
  an efficient software implementation.")

---

## 5. Free / legal homebrew ROMs and freely distributable demos for smoke-testing

Prefer the test ROMs in §3–§4 for correctness; use these to prove the emulator can actually
*boot and run a game*.

| URL | What it is | License / status |
|---|---|---|
| <https://github.com/mattcurrie/dmg-acid2/releases/download/v1.0/dmg-acid2.gb> | Minimal, self-contained, deterministic ROM that exercises the whole PPU. The best first "does it display anything?" target. | MIT (ROM prebuilt in releases, fetched as a downloadable asset). |
| <https://github.com/mattcurrie/cgb-acid2/releases/download/v1.1/cgb-acid2.gbc> | Same idea for CGB. | MIT. |
| <https://gekkio.fi/files/mooneye-test-suite/> | Prebuilt mooneye ROMs — the smallest possible MBC-less programs that still need a working cartridge header, timer, interrupts and PPU to terminate. | MIT. |
| <https://github.com/SimonLarsen/tobutobugirl> | "Tobu Tobu Girl" — a complete, polished homebrew platformer with full source; runs on real DMG. Good end-to-end smoke test (MBC, input, sprites, sound). | **MIT** (verified via GitHub API). |
| <https://github.com/InvisibleUp/AquaAndAshes> | "Aqua and Ashes" — a homebrew title with source; heavier than Tobu Tobu Girl. | **MIT** (verified via GitHub API). |
| <https://hh.gbdev.io> | Homebrew Hub: a community archive of unlicensed/homebrew GB software, playable in-browser. Use it to pick a demo, then get the ROM from the author's own repo. | Per-title; check the entry (site returned 403 to this client). |

**Not recommended / not verified:** commercial ROMs of any kind, including "abandonware" claims —
none of that is legal to redistribute, and nothing in this file depends on it.

**Note on blargg's and mooneye's ROMs as smoke tests:** they are not games, and blargg's tests
deliberately tolerate missing LCD support ("the VBlank wait routine has a timeout in case LY
doesn't reflect the current LCD line"), so a passing blargg test does **not** prove your PPU
works. Use dmg-acid2 for that.

---

## 6. Tools worth knowing

All of the following were verified to exist (HTTP 200 unless noted). Each entry is a real tool,
not a category.

### Emulators with debuggers

| URL | One-line annotation |
|---|---|
| <https://bgb.bircd.org/> | BGB — Windows emulator + debugger; the de-facto reference tool for "why does my ROM behave differently". Linked from awesome-gbdev as "Powerful emulator and debugger". |
| <https://sameboy.github.io/> | SameBoy — accurate emulator with "a wide range of powerful debugging features"; open source, so its `Core/` is a readable reference implementation. |
| <https://github.com/LIJI32/SameBoy/releases> | Prebuilt SameBoy binaries, including the CLI `sameboy_tester` used to script frame-hash comparison. |
| <https://emulicious.net/> | Emulicious — accurate emulator with profiler and source-level ASM/C debugging via a VS Code debug adapter. |
| <https://sameboy.github.io/automation/> | SameBoy's live automated test results — a way to check whether your interpretation of a quirk matches a well-tested implementation. |

### Tile / sprite / map viewers and converters

| URL | One-line annotation |
|---|---|
| <https://github.com/Rangi42/tilemap-studio> | Tilemap Studio — C++/FLTK tilemap editor for GB/GBC/GBA/SNES; the current go-to map editor. |
| <https://github.com/chrisantonellis/gbtdg> | Game Boy Tile Data Generator — browser tool that converts bitmap images into GB 2bpp tile data. |
| <http://www.devrs.com/gb/hmgd/intro.html> | Harry Mulder's GB pages: home of **Game Boy Tile Designer (GBTD)** and **Game Boy Map Builder (GBMB)** — old but still the classic viewers. |
| <http://www.devrs.com/gb/files/galp.zip> | Game Boy Assembly Language Primer — template code with memory defines, copy routines and a font tilemap. Useful as a known-good ROM to assemble. |

### Analysis helpers

| URL | One-line annotation |
|---|---|
| <https://github.com/rondnelson99/opcode_count> | opcode_count — collects per-opcode execution statistics from a ROM via Emulicious; handy for finding which instructions your emulator must get right first. |
| <https://gbhwdb.gekkio.fi/> | Game Boy Hardware Database — look up a specific console/mainboard/SoC revision when a test's expected result depends on it. |

**Not verified:** the specific feature sets claimed for closed-source debuggers were taken from
the awesome-gbdev descriptions, not from the vendors' own feature pages.

---

## 7. Communities

| URL | One-line annotation |
|---|---|
| <https://discord.gg/tKGMPNr> | The **gbdev Discord** invite (HTTP 200). Linked from awesome-gbdev's header as "Join us on Discord"; it is *the* place to ask "is this documented, or is it UNVERIFIED?" |
| <https://gbdev.io/chat.html> | The landing page that documents the Discord and other chat channels (403 to this client). |
| <https://github.com/gbdev/awesome-gbdev> | The "awesome" list: docs, emulators, tools, open-source ROMs, tutorials. Start here when looking for a tool not listed above. |
| <https://gbdev.gg8.se/forums/> | The long-running Game Boy Development Forum (HTTP 200) — searchable archive of pre-Discord hardware questions. |

---

## Verification summary

**Fetched and read successfully** (so facts drawn from them are verified):
`raw.githubusercontent.com/gbdev/pandocs/master/src/*` (Power_Up_Sequence, CPU_Instruction_Set,
CPU_Registers_and_Flags, Interrupts, Interrupt_Sources, Timer_and_Divider_Registers,
Timer_Obscure_Behaviour, Rendering, STAT, LCDC, Accessing_VRAM_and_OAM, OAM, OAM_DMA_Transfer,
OAM_Corruption_Bug, Memory_Map, Audio, halt), `gekkio.fi/files/gb-docs/gbctr.pdf` (all 176 pages
extracted), `Gekkio/gb-ctr` `opcodes.toml` + `timing.typ` + `chapter/cpu/instruction-set.typ` +
`appendix/opcode-tables.typ` + `chapter/peripherals/boot-rom.typ` + `LICENSE`,
`gbdev/gb-opcodes` `Opcodes.json` + `LICENSE`, `retrio/gb-test-roms` readmes,
`Gekkio/mooneye-test-suite` README + LICENSE + `acceptance/boot_hwio-dmgABCmgb.s`,
`Gekkio/mooneye-gb` README + LICENSE, `mattcurrie/dmg-acid2` README + LICENSE,
`mattcurrie/cgb-acid2` README, `LIJI32/SameBoy` LICENSE + `BootROMs/dmg_boot.asm` +
`.github/actions/sanity_tests.sh` + `Tester/main.c`, `pokemon-speedrunning/gambatte-core` README
+ tree, `gbdev/awesome-gbdev` README, GitHub API metadata for the repos cited.

**Could not fetch (HTTP 403 to this environment's client; URLs themselves are the canonical ones
the primary sources link to):** every `gbdev.io` and `rgbds.gbdev.io` path, and
`hh.gbdev.io`. Pan Docs content was obtained from its byte-identical source instead.

**Explicitly UNVERIFIED items:** blargg's ROM suite license; the exact DMG CPU datasheet
(not obtained); vendor feature lists for BGB/Emulicious beyond the awesome-gbdev one-liners;
the current `toruzz/Deadus`/Deadeus repository URL (a direct fetch failed, so it is omitted).
