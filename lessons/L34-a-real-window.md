# L34 — A real window

**Time** — theory ~20 min | coding ~50 min | verify ~20 min
**You will end with** — a game running at 60 fps in a window, with keys and sound. AHA:
*the emulator was finished before this lesson; this lesson is a program that uses it.*
**Tests that must go green** — none, and that is the design. The gate is a game you can
play. The suite must stay green *and stay free of any display dependency*.
**Depends on** — everything. You will create `gb/src/window.c`; the interface is already
declared in `gb/include/gb/window.h`.

---

## 1. Read this — the whole theory for today

### Frontend and backend: why this lesson is short

Since L01 the machine has been headless, and that is why the whole course was testable.
Every output the window needs is already a function of the core:

| The window needs | The core already gives |
| --- | --- |
| "advance a frame" | `gb_step(gb)` until `gb->frame_count` changes |
| pixels | `ppu_framebuffer(gb)` — 160x144 bytes, one shade 0..3 per pixel |
| sound | `apu_read_samples(gb, out, n)` — the APU's sample buffer |
| input | `joypad_set(gb, buttons)` — inject the button state |

So the window is a *loop with three jobs*: pace, present, collect input. There is no
emulator code in this lesson, and if you find yourself adding any — a flag in `gb_t`, an
SDL type in a component header — you have taken the wrong path. The reward for that
discipline is that the suite you have built keeps testing the machine you ship.

### Installing SDL2

```
pacman -S mingw-w64-ucrt-x86_64-SDL2
```

The `ucrt-x86_64` part is not optional. The plain `mingw-w64-x86_64-SDL2` package is built
against a different C runtime, and mixing it with your UCRT64 gcc gives link errors or
crashes that look like emulator bugs. This is the same ABI trap L24's lesson notes for gdb.

`build.ps1` and `Makefile` already detect SDL2 and define **`GB_WITH_SDL` for the emulator
only**. That detail is the whole reason this lesson can exist without damaging the course:

* `src/window.c` is guarded by `#ifdef GB_WITH_SDL`, so for the test binary it compiles to
  an empty file and no SDL header or library is needed;
* the emulator build defines the macro and links SDL2;
* so `-Test` stays dependency-free, and you can hand the emulator to someone without SDL
  and it still runs headless.

If you leave the guard out, the test build tries to compile SDL code and `-Test` stops
linking for everyone. The build prints which of the two paths it took:

```
SDL2: found at C:\msys64\ucrt64 - the window frontend will be built
SDL2: not found - building headless only (see PREREQUISITES.md)
```

### The loop

```c
int window_run(gb_t *gb, int scale)
{
    /* 1. create: SDL window (160*scale x 144*scale), renderer with nearest scaling,
     *    a streaming texture, and an audio device.
     *    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest") is what keeps the
     *    pixels square instead of blurry. */
    while (running) {
        /* 2. input: SDL_PollEvent -> a button bitmask -> joypad_set(gb, mask) */
        /* 3. advance exactly one frame: */
        u32 target = (u32)gb->frame_count + 1;
        while (gb->frame_count < target && !gb->fatal) gb_step(gb);
        /* 4. present: framebuffer -> ARGB pixels -> SDL_UpdateTexture -> RenderCopy */
        /* 5. audio: drain apu_read_samples and SDL_QueueAudio it */
        /* 6. pace: wait until 16.74 ms have passed since step 3 started */
    }
}
```

**Frame pacing.** The machine's frame rate is `59.7275 Hz`, so a frame is
`70224 / 4194304` seconds = **16.7427 ms**. Pace with `SDL_Delay` (or a busy-wait for the
last millisecond) against the time you took at the start of the frame. Do not pace by
instruction count and do not use `SDL_Delay(16)`: 16 ms is 59.7275 Hz's nearest integer, and
the drift is a visible 1 frame every ~8 seconds.

**Audio: queue from the main loop, not from the callback.** An SDL audio callback runs on a
**separate thread**, and your core is single-threaded — a callback calling `apu_read_samples`
while the main thread is inside `gb_step` is a data race on every APU register. The simple,
correct design is to drain the APU in the main loop (step 5 above) and hand the samples to
`SDL_QueueAudio`. If the queued backlog exceeds ~100 ms, the machine is running ahead of the
sound card: skip the audio push for that frame rather than growing a latency you can never
recover from.

**The palette.** Four shades, 0 = lightest:

| Shade | Classic grey | DMG green (the one you remember) |
| --- | --- | --- |
| 0 | `E0 E0 E0` | `E0 F8 D0` |
| 1 | `A0 A0 A0` | `88 C0 70` |
| 2 | `50 50 50` | `34 68 56` |
| 3 | `00 00 00` | `08 18 20` |

**Input mapping.** Arrows -> D-pad, `Z` -> A, `X` -> B, `Enter` -> Start, `Shift` -> Select,
`Escape` -> quit. `joypad_set` takes the bitmask from `gb/joypad.h`; send it once per frame
*before* stepping, so the machine sees a stable state for the whole frame.

> **What matters for the code you are about to write**
>
> * `src/window.c` must be wrapped in `#ifdef GB_WITH_SDL`. Without it, `-Test` stops
>   linking — the build globs `src/*.c` for both targets.
> * Call `window_run` from `main.c` under the same `#ifdef`, next to the existing
>   `--headless` handling. `--headless` must keep working with SDL installed, so the
>   existing headless path stays the default for `--frames`/`--max-cycles` runs.
> * Advance the machine **to the next frame boundary** by watching `gb->frame_count`, never
>   by a fixed number of `gb_step` calls: instruction counts vary with `HALT`, interrupts
>   and the joypad.
> * Check `gb->fatal` in the loop and print the trace when it trips, or a crashing game
>   turns the window into a frozen rectangle with no explanation.
> * Drain the APU in the main loop and use `SDL_QueueAudio`. Do **not** use an SDL audio
>   callback: it runs on another thread and the core is not thread-safe.
> * `SDL_HINT_RENDER_SCALE_QUALITY = "nearest"` and an integer scale, so 160x144 stays
>   crisp. `SDL_RenderSetLogicalSize` + `SDL_RenderCopy` handles the scaling for you.
> * No SDL type may appear in `gb.h` or any `include/gb/*.h`. The window header
>   (`gb/window.h`) is the only place that knows a window exists, and it does not include
>   SDL at all.
> * Do the same work in both build paths if you use the Makefile: it already has the
>   `pkg-config`-based hook; check it prints no SDL flags when SDL is missing.

---

## 2. Your task — 50 min

Create `gb/src/window.c`.

1. **Create and present** (20 min). Window, renderer, streaming texture, the palette
   conversion, and one frame drawn from `ppu_framebuffer(gb)`. Get a static picture on
   screen before adding timing.
2. **The frame loop and pacing** (15 min). Step to the next `frame_count`, pace to 16.7427 ms,
   and show the fps in the window title so you can see drift immediately.
3. **Input** (10 min). Poll events, build the bitmask, `joypad_set` once per frame. Escape
   quits.
4. **Audio** (10 min). Open an S16 device at 48000 Hz mono (or whatever
   `apu_sample_rate()` reports), drain the APU per frame, `SDL_QueueAudio`, and cap the
   backlog.
5. **Wire it up** (5 min). In `main.c`, under `#ifdef GB_WITH_SDL`, call `window_run(gb, 5)`
   when `--headless` was not given and neither `--frames` nor `--max-cycles` was.

---

## 3. Prove it — 20 min

```
.\gb\build.cmd
```
```
SDL2: found at C:\msys64\ucrt64 - the window frontend will be built
built: gb\build\gbemu.exe
```

Then, in order, because each step isolates a different failure:

```
:: 1. the picture
.\gb\build\gbemu.exe --rom roms\dmg-acid2.gb
:: 2. motion and input
.\gb\build\gbemu.exe --rom <a game>.gb
:: 3. the CLI still works, with SDL installed
.\gb\build\gbemu.exe --rom roms\dmg-acid2.gb --frames 60 --dump-frame build\w.bmp
python tools\compare_bmp.py build\w.bmp build\reference.bmp
:: 4. the suite still builds and passes, with no display
.\gb\build.cmd -Test
```

Step 3 is the one people skip and regret: the windowed and headless builds must produce the
same frame, which is the evidence that the frontend did not change the machine. Step 4 must
show the *same counts* as before this lesson.

Then play a game for a few minutes and watch for the three classic frontend problems:

| What you see | What it is |
| --- | --- |
| Sound crackles or drifts behind the picture | backlog not bounded, or the callback race |
| The game runs fast/slow by ~1% | you paced with `SDL_Delay(16)` |
| Input feels a frame late | you call `joypad_set` *after* stepping |

---

## 4. If it fails

| Symptom | Cause | Find it |
| --- | --- | --- |
| `-Test` fails to link, `undefined reference to SDL_...` | `src/window.c` is not guarded by `#ifdef GB_WITH_SDL` | guard the whole file |
| `window_run` is undefined when building `gbemu.exe` | SDL was not found, or the guard hides the body but `main.c` still calls it | `main.c`'s call must be guarded too |
| Link errors mentioning `WinMain` | `-lmingw32 -lSDL2main -lSDL2` order, or `main` not renamed by `SDL.h` | include `<SDL.h>` before `main` |
| A blank white window | the texture format does not match your 32-bit pixel writes | `SDL_PIXELFORMAT_ARGB8888`, 4 bytes per pixel |
| Blurry pixels | default linear scaling | `SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest")` |
| The game runs at the wrong speed | pacing against a fixed `SDL_Delay` | pace to 16.7427 ms measured per frame |
| It freezes after a few seconds | `gb->fatal` was set and the loop kept going | check `gb->fatal`, dump the trace |
| Audio garbage or a crash under load | you used the SDL callback and raced the core | `SDL_QueueAudio` from the main loop |
| The window appears but nothing moves | you step a fixed instruction count per frame and it is too small | step to the next `frame_count` |

---

## 5. Done when

- [ ] `.\gb\build\gbemu.exe --rom <a game>.gb` shows the game at full speed in a window
- [ ] The arrow keys, `Z`, `X`, `Enter` and `Shift` all do what they should
- [ ] Sound plays, and the window title shows a stable ~59.7 fps
- [ ] `--headless` and `--frames` still work with SDL installed
- [ ] `.\gb\build.cmd -Test` passes with the same counts, proving the frontend is not in the core
- [ ] No SDL type appears in `include/gb/*.h`
- [ ] Commit message like `feat(frontend): an SDL2 window, input and audio (L34)`

---

## 6. Optional, only if you have time

* Save states on a hotkey (`F5`/`F8`) now that L32 gave you the functions — the fastest bug
  reproduction loop there is.
* A second window showing the PPU's layers (BG, window, sprites) as separate bitmaps, using
  the debug dumps you added in L22. It is the tool you will want the next time a game's
  graphics break.

Next: **[L35 — The APU: the square channels](L35-the-apu-square-channels.md)**
