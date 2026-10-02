/*
 * window.h - the windowed frontend's interface. Yours to implement (L34).
 *
 * The window is a FRONTEND: it drives the machine and displays it, and it must not
 * be part of it. Everything it needs already exists, and every one of those calls
 * goes through a header you have been using since L01:
 *
 *   gb_step(gb)                  one instruction (+ its bus ticks)
 *   ppu_framebuffer(gb)          160x144 bytes, one shade 0..3 per pixel
 *   apu_read_samples(gb, out, n) drain the APU's sample buffer
 *   joypad_set(gb, buttons)      inject the button state for the next frame
 *
 * The contract for the implementation:
 *
 *   - src/window.c must be guarded by `#ifdef GB_WITH_SDL`. build.ps1 defines that
 *     macro for the emulator ONLY, so the test binary compiles the file to nothing
 *     and the suite keeps building with no display and no SDL installed. If you
 *     leave the guard out, `-Test` stops linking for everyone.
 *
 *   - window_run() runs until the user closes the window or presses Escape, and
 *     returns 0. Call it from main.c only under the same `#ifdef`, next to the
 *     existing `--headless` handling, so the CLI keeps working without SDL.
 *
 *   - The core stays headless: no SDL types may leak into gb.h or any component
 *     header. A windowed build and a headless build must produce identical
 *     framebuffers for the same input - that is what makes the whole test suite
 *     meaningful for a build you can also look at.
 *
 * `scale` is the integer pixel scale (4 or 5 is comfortable at 160x144).
 */
#ifndef GB_WINDOW_H
#define GB_WINDOW_H

#include "gb/common.h"

typedef struct gb_s gb_t;

/* Run the machine in a window until the user quits. Returns 0 on success, non-zero
 * if the window, the renderer or the audio device could not be created. */
int window_run(gb_t *gb, int scale);

#endif /* GB_WINDOW_H */
