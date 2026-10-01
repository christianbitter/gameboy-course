# roms/ — where your test ROMs live

This directory is git-ignored except for this file. Nothing here is provided:
ROMs are either copyrighted (commercial games, the boot ROM) or written by
someone else, and you should get them from the source. All of the ones below are
free to download and to run.

## The ones that matter, in the order you will need them

| ROM | What it proves | Milestone |
| --- | --- | --- |
| `blargg/cpu_instrs/individual/01-special.gb` .. `11-op a,(hl).gb` | every opcode and every flag | M05 |
| `blargg/cpu_instrs/cpu_instrs.gb` | all eleven at once | M05 |
| `blargg/instr_timing.gb` | instruction cycle counts | M09 |
| `blargg/mem_timing.gb`, `mem_timing-2.gb` | memory-access cycle counts | M09 |
| `blargg/halt_bug.gb` | the HALT bug and interrupt timing | M09 |
| `blargg/oam_bug.gb` | VRAM/OAM access restrictions | M09 |
| `blargg/dmg_sound/01-registers.gb` .. `12-wave write while on.gb` | the APU | M10 |
| `dmg-acid2.gb` | the whole PPU against a reference image | M07 |
| `mooneye/acceptance/*.gb`, `mooneye/bits/*.gb` | one hardware behaviour each | M05 onward |

Where to get them, with licences and current URLs:
`reference/external-references.md`. The short version:

* blargg's suite: the `gb-test-roms` repository (`cpu_instrs`, `instr_timing`,
  `mem_timing`, `halt_bug`, `oam_bug`, `dmg_sound`).
* mooneye-gb: the `mooneye-gb` test suite (`acceptance/`, `bits/`, `emulator-only/`).
* `dmg-acid2`: its own small repository. It draws a face; each feature of the face
  tests one rendering rule. Compare against the reference image in that repo.

## How to run them

```
.\gb\build.cmd

rem blargg prints its verdict on the serial port
.\gb\build\gbemu.exe --rom roms\blargg\cpu_instrs\individual\01-special.gb --serial - --frames 4000
rem expected:
rem   01-special
rem   Passed

rem dmg-acid2 is a picture, so dump a frame
.\gb\build\gbemu.exe --rom roms\dmg-acid2.gb --frames 30 --dump-frame build\acid2.bmp
start build\acid2.bmp

rem mooneye signals completion with LD B,B (opcode 0x40); --break-op stops there
.\gb\build\gbemu.exe --rom roms\mooneye\bits\unused_hwio-C.gb --break-op 40 --max-cycles 20000000
```

If serial prints nothing, the bug is in your serial implementation, not in the
CPU: check that writing `SC = 0x81` emits `SB`, leaves `SC = 0x01` and raises
`IF` bit 3 (see `docs/04` section 4.5).

## Free homebrew and demos worth having

Also in `reference/external-references.md`: small freely-licensed games and demos
that boot fast and exercise the PPU, sprites, scrolling and audio without the
size of a commercial ROM. Use one of them for the "it boots a game" milestone if
you have no cartridge dump of your own.

## Layout this course assumes

```
roms/
  blargg/
    cpu_instrs/individual/01-special.gb ...
    instr_timing.gb
    mem_timing.gb
    mem_timing-2.gb
    halt_bug.gb
    oam_bug.gb
    dmg_sound/01-registers.gb ...
  dmg-acid2.gb
  mooneye/
    acceptance/...
    bits/...
  homebrew/
    <something small>.gb
```

Keep it flat enough to type and consistent with the commands above; you will
retype these paths a hundred times.

## The boot ROM

Do not download one and do not ship one. It is copyrighted, and emulating it is
not required: this course skips it by default (`--no-boot-rom`) using the
documented post-boot state in `reference/cheatsheet-flags-and-timing.md`. If you
want the real thing for M12, dump it from hardware you own.

## Golden artifacts

Small artifacts you *do* want in git (adjust `.gitignore` with a `!` rule):

* a 160x144 PPM/BMP that a test ROM is supposed to produce (a golden frame),
* a hand-written 32 KiB test ROM you assembled yourself,
* a `trace.txt` of a known-good run you keep coming back to.
