# Prerequisites

The course needs **one C compiler, one Python interpreter, and a terminal**. That
is the whole list for milestones M00-M10. Everything else is either optional or
explicitly unnecessary, and both categories are spelled out below so you never
have to guess whether you are missing something.

## Check your machine in one command

```
python tools/check_env.py
```

It reports every item below as `[ OK ]`, `[MISS]` (required and missing) or `[ -- ]`
(optional and missing), prints the install command for whatever is absent, and
exits non-zero only when a **required** item is missing. Nothing is installed or
downloaded by it.

## Required before milestone M00

| Tool | Why this course needs it | Verify with | Install (Windows) |
| --- | --- | --- | --- |
| **gcc, C17** | The emulator is C17. The test harness uses GCC's `__attribute__((constructor))` to auto-register tests, so it is gcc (or clang), **not MSVC**. | `gcc --version` and `gcc -dumpmachine` -> `x86_64-w64-mingw32` | MSYS2, then `pacman -S mingw-w64-ucrt-x86_64-gcc` |
| **Python 3.8+** | The tooling: `tools/opcode_table.py`, `tools/check_links.py`, `tools/check_env.py`, and the differential fuzzer you may write in M12. | `python --version` | python.org or `winget install Python.Python.3.12` |
| **A terminal** | Every acceptance command is run from the course root. | `cd gameboy-course` | cmd.exe or PowerShell, already present |
| **An editor** | You will write ~2,000 lines of C. | it can save **UTF-8 without BOM** | **VS Code** — this course ships a `.vscode/` setup for it (see below). Notepad++ or Vim work too. |

**git is strongly recommended, and the only "required" item that is not strictly
mechanical:** milestone M00 has you initialise a repository. You can skip it, but
then you lose the ability to bisect a regression — which is how you will find the
M09 timing bugs — and you lose the `.gitignore` that keeps 10 MB of ROMs and BMPs
out of your history.

```powershell
# the two commands that matter, on a machine with MSYS2 but no PATH entry yet
setx PATH "$env:PATH;C:\msys64\ucrt64\bin"      # then open a NEW shell
gcc -dumpmachine                                 # must print x86_64-w64-mingw32
```

### Install MSYS2 correctly (the one thing people get wrong)

Install MSYS2, then install the **UCRT64** toolchain:

```
pacman -S mingw-w64-ucrt-x86_64-gcc
```

There are three MSYS2 environments (`msys`, `mingw64`, `ucrt64`) whose binaries are
**not** interchangeable. Use `ucrt64` for everything. If you later mix an SDL2 built
for `mingw64` with a `ucrt64` gcc you get link errors at best and a program that
starts and misbehaves at worst — see M11.

## VS Code on Windows (already configured)

Open the **`gameboy-course` folder itself** as the workspace root (File -> Open
Folder). Everything in `.vscode/` is committed and written for this layout. If you
open a parent folder instead, the `${workspaceFolder}` paths in the tasks and debug
configurations will not resolve — that is the single most common "the config is
broken" report.

Install the extensions VS Code recommends (or `Ctrl+Shift+P` ->
"Extensions: Show Recommended Extensions"):

| Extension | Why |
| --- | --- |
| `ms-vscode.cpptools` | IntelliSense, the `$gcc` problem matcher, debugger integration |
| `ms-vscode.hexeditor` | opens `.gb` and `.sav` as hex, which you will want in M01 and M08 |
| `ms-python.python` | nothing is required, but the `tools/` scripts get linting and a Run button |

You do **not** need the C/C++ Extension Pack, CMake Tools, or any build-system
extension: every compiler invocation is one gcc command line (see
`unwantedRecommendations` in `.vscode/extensions.json`).

### Tasks — `Ctrl+Shift+B`, or `Ctrl+Shift+P` -> "Tasks: Run Task"

| Task | What it does |
| --- | --- |
| `1 build (debug)` | the default build task: `-g -O0`, gcc errors land in the Problems panel |
| `2 build (release)` | `-O2` — for real-time play only, never for debugging |
| `3 test: all` | the full suite in a shared terminal |
| `4 test: milestone (prompt)` | asks for a filter substring (`m00`, `m03_alu`, `m07_`) |
| `5 test: from scratch` | deletes `gb/build`, rebuilds, tests |
| `6 run ROM: serial to terminal` | the blargg workflow: `--serial -`, verdict on stdout |
| `7 run ROM: 600 frames + dump` | writes `gb/build/frame.bmp` and `gb/build/trace.txt` |
| `8 check: prerequisites` | `tools/check_env.py` |
| `9 check: links` | `tools/check_links.py` |
| `10 check: opcodes vs your C table` | lists base opcodes `src/opcodes.c` has not mentioned yet |
| `11 generate: opcode map markdown` | writes `gb/build/opcodes.md` — all 512 opcodes as a table |

All of them shell out to `powershell -ExecutionPolicy Bypass`, so the execution
policy that blocks `.ps1` by hand does not affect the tasks.

### Debugging — `F5`

Three launch configurations are provided. They need gdb, which is **not installed
on your machine yet**:

```
pacman -S mingw-w64-ucrt-x86_64-gdb
```

* **Debug tests: all** and **Debug tests: filtered** run the test binary under gdb.
  The harness matters here: a failing assertion `longjmp`s back to the runner
  instead of crashing, and `GB_UNIMPLEMENTED` is caught the same way. So set a
  breakpoint **inside the function you are implementing** (`src/opcodes.c`,
  `src/ppu.c`, ...) rather than waiting for a fault you will never get.
* **Debug gbemu on a ROM** runs the emulator with `--break-op 40`, so a
  mooneye-style `LD B,B` stop halts execution where you can inspect registers.

`miDebuggerPath` points at `C:/msys64/ucrt64/bin/gdb.exe`. If your MSYS2 lives
elsewhere, edit that one line in `.vscode/launch.json` and `compilerPath` in
`.vscode/c_cpp_properties.json`.

### IntelliSense

`c_cpp_properties.json` already puts `gb/include` on the include path, so
`#include "gb/gb.h"` resolves from any source file, with `cStandard: c17` and
`intelliSenseMode: windows-gcc-x64`. If you still see red squiggles:
`Ctrl+Shift+P` -> "C/C++: Select IntelliSense Configuration" -> "MSYS2 UCRT64 (gcc,
C17)", then "C/C++: Reset IntelliSense Database".

### Why `settings.json` pins `files.encoding: utf8`

Because M00's most common editor trap is this specific editor: VS Code will happily
save a file as UTF-16, after which gcc reports `stray '\377' in program`. The
committed setting removes that failure mode for the whole project, and it also
associates `.gb`/`.sav` with the hex editor and hides `build/` from the explorer.

## Required later — do not install these now

| Tool | Needed at | Why | Install |
| --- | --- | --- | --- |
| **SDL2** | M11 | the interactive window. Until then the front end is BMP/PPM dumps, which is deliberate: the PPU is verified against a file, not against your eyes. | `pacman -S mingw-w64-ucrt-x86_64-SDL2` |
| **ffmpeg** | M10/M11 | WAV spectrum plots, and turning frame dumps into a video you can watch. | `winget install Gyan.FFmpeg` |
| **gdb** | any | sometimes faster than the tracer; the tracer is still the primary tool. | `pacman -S mingw-w64-ucrt-x86_64-gdb` |
| **RGBDS** | optional | assembler, if you want to write your own test ROMs instead of downloading them. | `pacman -S mingw-w64-ucrt-x86_64-rgbds` |
| **A reference emulator** (BGB, SameBoy, Emulicious) | M07 onward | the practical oracle for "my ROM does X, yours does Y". `docs/06` §6.1 puts it third in the oracle hierarchy, after hardware and test ROMs. | see `reference/external-references.md` |

## Explicitly NOT needed

| Not needed | Because |
| --- | --- |
| `make`, `cmake`, `ninja`, `bazel` | `gb\build.cmd` and `gb\build.ps1` invoke gcc directly. A `Makefile` exists for WSL/Linux/macOS, and you never have to install a build system on Windows. |
| MSVC, `cl.exe`, Visual Studio | the test harness uses GCC attributes; the build scripts assume gcc. |
| Any `pip` package, virtualenv, or conda env | every tool in `tools/` uses only the Python standard library (`argparse`, `os`, `re`, `sys`). There is nothing to install. |
| Node.js, Java, .NET, Docker | irrelevant to this project. |
| A GitHub account or any network access after setup | the course is complete offline. Only the test ROMs (M05 onward) need one download, and they are optional until then. |
| A commercial ROM | M00-M06 are fully exercisable with the hand-built ROM images the test fixtures create for you. |
| A physical Game Boy, flashcart, or hardware debugger | nice to have, never required. |

## Platform notes

* **Windows** (what this course was written on): use `.\gb\build.cmd ...`.
* **WSL / Linux / macOS**: use `make`, `make test`, `make test FILTER=m03`, and
  replace `.\` with `./`. Nothing else changes; the Python tools are portable and
  `tools/check_env.py` knows the Unix SDL2 include paths.
* The only Windows-specific files in the project are `gb/build.ps1`, `gb/build.cmd`
  and the `.\` prefixes in the commands. There is no platform-specific code in the
  emulator itself.

## Troubleshooting

| Symptom | Cause | Fix |
| --- | --- | --- |
| `gcc: command not found`, or the build script prints "gcc not found" | MSYS2's `ucrt64\bin` is not on `PATH` | `setx PATH "$env:PATH;C:\msys64\ucrt64\bin"`, then open a **new** shell |
| Link errors mentioning `libgcc`, `stdint.h` or a missing `__udivdi3` | a *different* gcc is first on `PATH` (WSL gcc, an old MinGW, clang) | `where.exe gcc` lists every candidate; `gcc -dumpmachine` must print `x86_64-w64-mingw32` |
| `build.ps1 cannot be loaded because running scripts is disabled` | PowerShell execution policy | use `.\gb\build.cmd` (it bypasses the policy for that one process), or `powershell -NoProfile -ExecutionPolicy Bypass -File .\gb\build.ps1` |
| `python: command not found` but you installed Python | only the `py` launcher is on `PATH` | use `py tools\check_env.py`; or add Python's install directory to `PATH` |
| SDL2 links, then the window crashes or hangs at startup | you installed `mingw-w64-**x86_64**-SDL2` for a `ucrt64` gcc | `pacman -S mingw-w64-ucrt-x86_64-SDL2` (matching prefixes) |
| `error: stray '\377' in program` on a file you just wrote | the editor saved it as UTF-16 | re-save as UTF-8; `Format-Hex gb\src\cpu.c \| Select-Object -First 1` shows a `FF FE` BOM if so |
| `python tools\check_env.py` says the course tree is incomplete | you copied only part of the directory | re-clone or re-extract the whole `gameboy-course/` tree |
| `F5` -> "Unable to start debugging. The value of miDebuggerPath is invalid" | gdb is not installed | `pacman -S mingw-w64-ucrt-x86_64-gdb`, or correct the path in `.vscode/launch.json` |
| `F5` -> "The preLaunchTask ... failed" or "program does not exist" | the debug config builds first, and the build failed | read the gcc error in the terminal, fix it, then `F5` again |
| Red squiggles on `#include "gb/gb.h"` | cpptools is not using the shipped configuration | "C/C++: Select IntelliSense Configuration" -> "MSYS2 UCRT64 (gcc, C17)" |
| A task runs in the wrong folder | you opened a parent folder instead of `gameboy-course` | File -> Open Folder -> `gameboy-course` (the `${workspaceFolder}` paths depend on it) |
| `.\gb\build.cmd` fails in the VS Code terminal | the terminal is a WSL or MSYS2 shell, not PowerShell/cmd | open a Windows terminal (`Terminal -> Split Terminal`, or select PowerShell from the `+` dropdown) |

## Verified state of the machine this course was built on

```
[ OK ] REQUIRED gcc (C17)             gcc 15.2.0  [x86_64-w64-mingw32]   (MSYS2 UCRT64)
[ OK ] REQUIRED python (tooling)      python 3.11.0
[ OK ] REQUIRED git                   git cmd\git.exe
[ OK ] REQUIRED course tree           complete
[ -- ] optional make (optional)       not installed - use gb\build.cmd instead
[ -- ] optional cmake (optional)      not needed - the course ships no CMake files
[ OK ] optional ffmpeg (optional)     present
[ -- ] optional SDL2 (optional, M11)  not installed - needed only for the windowed front end
[ -- ] optional gdb (optional)        not installed
```

Optional gaps are fine and intentional: `make`/`cmake` are never called, SDL2 is not
needed until M11, and the tracer replaces gdb for 95% of debugging.

## Why there is no Dev Container or installer script

Because the requirement is a single `gcc` invocation and a stdlib-only Python
script. A container would add a layer you cannot inspect, and this course is
explicitly about being able to reason about every step between source and pixel.
If you want reproducibility for a team, the honest answer is: pin gcc's version in
your CI and keep the build to one command, which is what `gb/build.ps1` already is.

Next: **[README.md](README.md)** (what you are building) then
**[TASKS.md](TASKS.md)** (what to do first).
