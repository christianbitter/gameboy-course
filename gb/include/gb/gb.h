/*
 * gb.h - the machine. One struct, one step function.
 *
 * This is the umbrella header: include it and you have everything.
 */
#ifndef GB_GB_H
#define GB_GB_H

#include "gb/common.h"
#include "gb/cpu.h"
#include "gb/bus.h"
#include "gb/cart.h"
#include "gb/timer.h"
#include "gb/ppu.h"
#include "gb/joypad.h"
#include "gb/serial.h"
#include "gb/apu.h"
#include "gb/debug.h"

/*
 * The whole machine.
 *
 * Design rule: state lives here, behaviour lives in the component .c files,
 * and everything shares the clock through gb_step() -> bus_tick().
 */
struct gb_s {
    cpu_t    cpu;
    bus_t    bus;
    cart_t   cart;
    timer_t  timer;
    ppu_t    ppu;
    joypad_t joypad;
    serial_t serial;
    apu_t    apu;

    /* Boot ROM. While boot_rom_enabled is true, 0000-00FF is served from here
     * instead of the cartridge. Skipping it is fine and is the default. */
    u8   boot_rom[0x100];
    bool boot_rom_loaded;
    bool boot_rom_enabled;

    bool frame_ready;   /* raised by the PPU, consumed by gb_run_frame()  */
    bool fatal;         /* set by cpu_fatal(); the machine stops          */
    bool headless;      /* no window: run flat out                        */

    u64 total_ticks;    /* master clock ticks since reset                 */
    u64 frame_count;

    char rom_path[512];
    char save_path[520];

    FILE *serial_out;   /* NULL = discard serial output                   */

    trace_t trace;
};

/* ---- machine lifecycle (provided, src/gb.c) ------------------------------ */

gb_t *gb_create (void);
void  gb_destroy(gb_t *gb);

bool gb_load_rom_file(gb_t *gb, const char *path);  /* reads the file, calls
                                                       cart_load(), sets paths */

void gb_reset(gb_t *gb);        /* full machine reset, then post-boot state */

/*
 * Execute one instruction (or dispatch one interrupt) and advance every other
 * component by the same number of T-cycles. Returns the T-cycles consumed.
 */
u32  gb_step(gb_t *gb);

/* Run until the PPU finishes a frame (or until gb->fatal). */
void gb_run_frame(gb_t *gb);

/* ---- things you implement ------------------------------------------------ */

/*
 * gb_apply_post_boot_state (implement in src/cpu.c, milestone M06):
 * set the documented post-boot register and I/O values so the machine looks
 * like the boot ROM just handed control to 0x0100. See
 * reference/cheatsheet-flags-and-timing.md for the table.
 */
void gb_apply_post_boot_state(gb_t *gb);

/* Provide a file reader used by gb_load_rom_file: returns a malloc'd buffer. */
u8  *gb_read_file(const char *path, size_t *out_size);

/* ---- host services (provided, src/screenshot.c) -------------------------- */

void gb_write_bmp(const char *path, const u8 *framebuffer);
void gb_write_ppm(const char *path, const u8 *framebuffer);
extern const u8 GB_SHADES[4][3];

#endif /* GB_GB_H */
