#!/usr/bin/env python3
"""
check_env.py - verify the development prerequisites for this course.

    python tools/check_env.py
    python tools/check_env.py --verbose

Checks the tools the course actually needs, separates REQUIRED from OPTIONAL,
and prints the exact install command for anything missing. Exit code is 0 when
every required item is present, 1 otherwise.

Uses only the Python standard library - no pip installs, by design, because the
course must bootstrap itself before you can trust anything else in it.
"""

import argparse
import os
import shutil
import subprocess
import sys

WINDOWS = os.name == "nt"

# Searching PATH first is right; these are the fallbacks when MSYS2 is installed
# but its bin directory was never added to PATH - the single most common setup bug.
GCC_FALLBACKS = [
    r"C:\msys64\ucrt64\bin\gcc.exe",
    r"C:\msys64\mingw64\bin\gcc.exe",
]
SDL2_HINTS = [
    r"C:\msys64\ucrt64\include\SDL2\SDL.h",
    r"C:\msys64\mingw64\include\SDL2\SDL.h",
    "/usr/include/SDL2/SDL.h",
    "/usr/local/include/SDL2/SDL.h",
]


def find(exe, fallbacks=()):
    found = shutil.which(exe)
    if found:
        return found
    for candidate in fallbacks:
        if os.path.exists(candidate):
            return candidate
    return None


def probe(args):
    """Run a command and return its first output line, or None if it will not run."""
    try:
        result = subprocess.run(args, capture_output=True, text=True, timeout=15)
    except Exception:
        return None
    if result.returncode != 0:
        return None
    text = (result.stdout or result.stderr or "").strip().splitlines()
    return text[0].strip() if text else ""


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[1])
    ap.add_argument("--verbose", action="store_true")
    args = ap.parse_args()

    # (required?, name, ok, detail, fix)
    rows = []

    def add(required, name, ok, detail, fix=""):
        rows.append((required, name, ok, detail, fix))

    # ---- required ---------------------------------------------------------
    gcc = find("gcc", GCC_FALLBACKS)
    if gcc:
        version = probe([gcc, "--version"]) or "version unknown"
        target = probe([gcc, "-dumpmachine"]) or ""
        detail = version.replace("gcc.exe", "gcc")
        if target:
            detail += "  [%s]" % target
        add(True, "gcc (C17)", True, detail)
    else:
        add(True, "gcc (C17)", False, "not found",
            r'pacman -S mingw-w64-ucrt-x86_64-gcc  (then add C:\msys64\ucrt64\bin to PATH)')

    py_ok = sys.version_info >= (3, 8)
    add(True, "python (tooling)", py_ok, "python %d.%d.%d"
        % (sys.version_info[0], sys.version_info[1], sys.version_info[2]),
        "install Python 3.8+ (3.11 or newer recommended); no pip packages are needed")

    git = find("git")
    add(True, "git", bool(git), git or "not found (milestone M00 wants git init)",
        "winget install Git.Git   (optional: the course works without version control)")

    # ---- the course tree itself -------------------------------------------
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    missing = [p for p in ("README.md", "TASKS.md", "docs", "lessons", "gb/include/gb",
                           "gb/src", "gb/tests", "reference", "tools")
               if not os.path.exists(os.path.join(root, p))]
    add(True, "course tree", not missing,
        "complete" if not missing else "missing: " + ", ".join(missing),
        "re-clone or restore the course directory")

    # ---- optional ---------------------------------------------------------
    make = find("make") or find("mingw32-make")
    add(False, "make (optional)", bool(make), make or "not installed - use gb\\build.cmd instead",
        r'pacman -S make    (on WSL/Linux/macOS make is normally present)')

    cmake = find("cmake")
    add(False, "cmake (optional)", bool(cmake), cmake or "not needed - the course ships no CMake files",
        r'pacman -S cmake   (only if you add your own build system)')

    ffmpeg = find("ffmpeg")
    add(False, "ffmpeg (optional)", bool(ffmpeg),
        ffmpeg or "not installed - used for WAV/spectrum checks and frame-to-video",
        "winget install Gyan.FFmpeg")

    sdl = next((p for p in SDL2_HINTS if os.path.exists(p)), None)
    add(False, "SDL2 (optional, L34)", bool(sdl),
        sdl or "not installed - needed only for the windowed front end",
        r'pacman -S mingw-w64-ucrt-x86_64-SDL2   (UCRT64, to match your gcc)')

    gdb = find("gdb")
    add(False, "gdb (optional)", bool(gdb), gdb or "not installed - the tracer is the main debugger",
        r'pacman -S mingw-w64-ucrt-x86_64-gdb')

    # ---- report -----------------------------------------------------------
    width = max(len(r[1]) for r in rows)
    print("prerequisites for the Game Boy emulator course")
    print("(run from %s)\n" % root)

    for required, name, ok, detail, fix in rows:
        tag = "REQUIRED" if required else "optional"
        mark = "[ OK ]" if ok else ("[MISS]" if required else "[ -- ]")
        print("%s %-8s %-*s  %s" % (mark, tag, width, name, detail))
        if not ok and (required or args.verbose):
            print("%34s fix: %s" % ("", fix))

    hard = [r for r in rows if r[0] and not r[1] and r[2] is False]
    soft = [r for r in rows if not r[0] and not r[1] and r[2] is False]
    if args.verbose and soft:
        print("\n%d optional item(s) missing: nothing is blocked, but you will want them later."
              % len(soft))
    if hard:
        print("\n%d REQUIRED item(s) missing. Fix those before milestone M00."
              % len(hard))
        return 1

    print("\nAll required prerequisites are present. Next:  .\\gb\\build.cmd -Test m00")
    if soft:
        print("%d optional item(s) missing - see PREREQUISITES.md for when each one matters."
              % len(soft))
    return 0


if __name__ == "__main__":
    sys.exit(main())
