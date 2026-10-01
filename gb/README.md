# `gb/` — the emulator project

```
include/gb/common.h    types, flag bits, machine constants, GB_UNIMPLEMENTED   GIVEN
include/gb/gb.h        struct gb_s: the whole machine                           GIVEN
include/gb/cpu.h        cpu_t, register accessors, the functions you implement  GIVEN (structs)
include/gb/bus.h        bus_t, the I/O ownership table, GB_IF/GB_IE            GIVEN (structs)
include/gb/cart.h       cart_t, header tables, cart_* signatures                GIVEN (structs)
include/gb/timer.h      timer_t                                                 GIVEN (structs)
include/gb/ppu.h        ppu_t, framebuffer                                      GIVEN (structs)
include/gb/joypad.h     joypad_t, button enum                                   GIVEN (structs)
include/gb/serial.h     serial_t                                                GIVEN (structs)
include/gb/apu.h        apu_t                                                   GIVEN (structs)
include/gb/debug.h      trace_t, gb_disasm, gb_hexdump                          GIVEN

src/main.c              CLI, file loading, dumps, --break-op                     GIVEN
src/gb.c                gb_create/step/run_frame/reset, serial hook, fatal path  GIVEN
src/debug.c             tracer ring buffer, hexdump, placeholder disassembler    GIVEN
src/screenshot.c        BMP and PPM writers                                      GIVEN

src/cpu.c               cpu_step, fetch, r8 helpers, post-boot state       YOURS  M02/M05/M06
src/opcodes.c           the opcodes                                        YOURS  M02/M03/M04
src/bus.c               the address decoder                                YOURS  M01
src/cart.c              header, MBCs, saves                                YOURS  M01/M08
src/timer.c             DIV/TIMA/TMA/TAC                                   YOURS  M05
src/ppu.c               modes, LY, rendering                               YOURS  M06/M07
src/joypad.c            the FF00 matrix                                    YOURS  M06
src/serial.c            SB/SC                                              YOURS  M05
src/apu.c               the four channels                                  YOURS  M10

tests/harness.h         TEST/TEST_ASSERT/TEST_EQ/TEST_TODO                   GIVEN
tests/test_main.c       runner + fixtures (t_machine, t_exec, t_build_rom)     GIVEN
tests/t_m00..t_m06      the M00-M06 acceptance tests (red until implemented)   GIVEN
tests/t_m07..t_m12      TEST_TODO stubs: YOU write these tests                 YOURS
```

## Build and test

```
.\gb\build.ps1                  # build both binaries
.\gb\build.ps1 -Test            # build and run everything
.\gb\build.ps1 -Test m03        # only tests whose name contains m03
.\gb\build.ps1 -Clean -Test     # from scratch
```
`make` users (WSL/Linux/macOS): `make`, `make test`, `make test FILTER=m03`.

If `gcc` is not on PATH the script looks in `C:\msys64\ucrt64\bin`. To add it
permanently: `setx PATH "$env:PATH;C:\msys64\ucrt64\bin"` then reopen the shell.

## How to read a failing run

```
[ RUN      ] m03_alu_add_flags
[  FAILED  ] m03_alu_add_flags
             C:\...\src\opcodes.c:214: UNIMPLEMENTED at src/opcodes.c:214: op_execute(opcode=80)
[       OK ] m00_smoke_builds
...
   3 passed,   27 failed,   8 skipped  (38 tests registered)
NOT GREEN YET - that is the point. Work the failures in milestone order.
```

* `UNIMPLEMENTED at file:line` means code does not exist yet. That is expected
  and tells you exactly where to type.
* A failure with `expected 0x20, got 0x00` is a real bug: your code ran and got
  the wrong answer. Those are the interesting ones.
* `SKIPPED` tests are stubs you have not written yet (M07 and later).
* The run never aborts on a missing function, so one green milestone cannot be
  hidden by a crash in the next one.

## Run a ROM

```
.\gb\build.ps1
.\gb\build\gbemu.exe --rom roms\blargg\cpu_instrs\individual\01-special.gb --serial - --frames 4000
.\gb\build\gbemu.exe --rom roms\blargg\dmg-acid2.gb --frames 2 --dump-frame build\acid2.bmp
.\gb\build\gbemu.exe --rom game.gb --info
.\gb\build\gbemu.exe --rom roms\mooneye\bits\unused_hwio-C.gb --break-op 40 --max-cycles 20000000
.\gb\build\gbemu.exe --rom game.gb --trace build\trace.txt --frames 600
```

`--dump-frame` writes a BMP you can double-click. `--trace` writes one line per
executed instruction; the last 16 lines are also dumped automatically when the
machine faults.

## Rules that keep this project moving

1. `cpu_step()` is the only place that advances the CPU, `bus_tick()` the only
   place that advances everything else. Do not invent a second path.
2. Unimplemented code aborts loudly (`GB_UNIMPLEMENTED`). Do not silently treat
   an opcode as NOP.
3. Every milestone has named tests. A milestone is done when its tests are green
   **and** its external gate passes (see `TASKS.md`).
4. Never call `bus_read()` with a cached value in mind: I/O reads have side
   effects, and reading `LY` twice can return two different numbers.
5. Add a test before fixing a bug, or you will meet that bug again in M09.
