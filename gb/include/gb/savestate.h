/*
 * savestate.h - serialising the whole machine. Yours to implement (L32).
 *
 * A save state is a snapshot of everything that makes the machine what it is, so
 * that loading it resumes exactly where saving left off. "Everything" means every
 * field of gb_t that changes while the machine runs:
 *
 *   cpu      all registers, pc, sp, ime/ime_pending/halted/halt_bug, the counters
 *   bus      vram, wram, oam, hram, io, ie, the DMA transfer state
 *   cart     ram (and the MBC latches: rom_bank, ram_bank, mode, ram_enabled)
 *   timer    div_counter, tima, tma, tac, the reload-delay state
 *   ppu      every register, dot, mode, window_line, stat_line, the framebuffer
 *   joypad   select, pressed, prev_read
 *   serial   sb, sc, the transfer state
 *   apu      the register file, wave RAM, and your channel state
 *   machine  total_ticks, frame_count
 *
 * What it must NOT contain: host config (serial_out, headless, paths), the ROM
 * image (it comes from the .gb file), and the tracer ring (debug scaffolding).
 * Storing the ROM would make every state file 8 MiB; storing a value you did not
 * restore is how a save state "works" but leaves a game subtly wrong.
 *
 * Two contracts the tests rely on:
 *
 *   - gb_save() returns the number of bytes written, or 0 if `cap` is too small.
 *     It must not write past `cap`, and it must not partially write and then fail.
 *   - gb_load() returns false for a buffer it did not produce (wrong length, wrong
 *     magic) and must leave the machine untouched in that case. A version tag in
 *     your format is the easy way to get this right.
 *
 * The simplest correct format is a small header (magic + version + length) followed
 * by the fields in a fixed order, written through a growable cursor rather than a
 * chain of fwrite calls. Keep it your own; the tests only check round-trip fidelity.
 */
#ifndef GB_SAVESTATE_H
#define GB_SAVESTATE_H

#include "gb/common.h"

typedef struct gb_s gb_t;

/* Serialise the machine into `buf`. Returns the bytes written, 0 on failure. */
size_t gb_save(const gb_t *gb, u8 *buf, size_t cap);

/* Restore a state produced by gb_save. Returns false and mutates nothing on a
 * buffer that is not one of ours. */
bool gb_load(gb_t *gb, const u8 *buf, size_t len);

#endif /* GB_SAVESTATE_H */
