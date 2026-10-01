/*
 * gb.c - machine lifecycle and the fatal-error plumbing.
 *
 * PROVIDED INFRASTRUCTURE. You should not need to edit this file. The three
 * functions that decide your architecture are here; read them once:
 *
 *   gb_step()      one instruction + advance every component by the same time
 *   gb_run_frame() run until the PPU completes a frame
 *   gb_reset()     wipe machine state, keep the cartridge and host config
 */
#include "gb/gb.h"
#include <stdarg.h>

/* The machine currently stepping, so a fatal error can dump its trace. */
static gb_t *g_gb_current;

static void default_fatal_handler(const char *message)
{
    fprintf(stderr, "\n[gb] FATAL: %s\n", message);
    if (g_gb_current) {
        fputs("--- last instructions (oldest first) ---\n", stderr);
        gb_trace_print_last(g_gb_current, stderr, 16);
    }
    fflush(stderr);
    abort();
}

gb_fatal_handler_t g_gb_fatal_handler = default_fatal_handler;

void gb_unimplemented_(const char *file, int line, const char *fmt, ...)
{
    char what[512];
    char msg[640];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(what, sizeof what, fmt, ap);
    va_end(ap);

    snprintf(msg, sizeof msg, "UNIMPLEMENTED at %s:%d: %s", file, line, what);
    g_gb_fatal_handler(msg);
}

/* -------------------------------------------------------------------------- */

u8 *gb_read_file(const char *path, size_t *out_size)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;

    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long size = ftell(f);
    if (size <= 0) { fclose(f); return NULL; }
    rewind(f);

    u8 *data = (u8 *)malloc((size_t)size);
    if (!data) { fclose(f); return NULL; }

    size_t got = fread(data, 1, (size_t)size, f);
    fclose(f);
    if (got != (size_t)size) { free(data); return NULL; }

    if (out_size) *out_size = got;
    return data;
}

static void make_save_path(const char *rom_path, char *out, size_t cap)
{
    snprintf(out, cap, "%s", rom_path);
    size_t len = strlen(out);
    if (len >= 3 &&
        (out[len - 3] == '.') &&
        (out[len - 2] == 'g' || out[len - 2] == 'G') &&
        (out[len - 1] == 'b' || out[len - 1] == 'B')) {
        out[len - 3] = '\0';
    }
    size_t l2 = strlen(out);
    if (l2 + 4 < cap) snprintf(out + l2, cap - l2, ".sav");
}

gb_t *gb_create(void)
{
    gb_t *gb = (gb_t *)calloc(1, sizeof *gb);
    if (!gb) return NULL;

    gb_trace_init(&gb->trace);
    cpu_reset(gb);
    timer_reset(gb);
    ppu_reset(gb);
    serial_reset(gb);
    joypad_reset(gb);
    apu_reset(gb);

    gb->serial_out = NULL;
    return gb;
}

void gb_destroy(gb_t *gb)
{
    if (!gb) return;
    cart_unload(gb);
    if (gb->trace.file) fclose(gb->trace.file);
    if (gb->serial_out && gb->serial_out != stdout && gb->serial_out != stderr) {
        fclose(gb->serial_out);
    }
    free(gb);
}

bool gb_load_rom_file(gb_t *gb, const char *path)
{
    size_t size = 0;
    u8 *data = gb_read_file(path, &size);
    if (!data) {
        fprintf(stderr, "error: cannot read ROM '%s'\n", path);
        return false;
    }
    if (!cart_load(gb, data, size, path)) {
        free(data);
        return false;
    }
    free(data);   /* cart_load copies what it needs */

    snprintf(gb->rom_path, sizeof gb->rom_path, "%s", path);
    make_save_path(path, gb->save_path, sizeof gb->save_path);
    return true;
}

void gb_reset(gb_t *gb)
{
    /* Machine state is wiped; the cartridge, the boot ROM and host config are
     * preserved. Order matters: reset components, then apply the state the
     * boot ROM would have left behind. */
    cart_t   saved_cart = gb->cart;
    u8       saved_boot[0x100];
    bool     boot_loaded = gb->boot_rom_loaded;
    FILE    *serial_out  = gb->serial_out;
    bool     headless    = gb->headless;
    char     rom_path[512];
    char     save_path[520];

    memcpy(saved_boot, gb->boot_rom, sizeof saved_boot);
    memcpy(rom_path, gb->rom_path, sizeof rom_path);
    memcpy(save_path, gb->save_path, sizeof save_path);

    memset(gb, 0, sizeof *gb);

    gb->cart            = saved_cart;
    memcpy(gb->boot_rom, saved_boot, sizeof saved_boot);
    gb->boot_rom_loaded = boot_loaded;
    gb->boot_rom_enabled = boot_loaded;
    gb->serial_out      = serial_out;
    gb->headless        = headless;
    memcpy(gb->rom_path, rom_path, sizeof rom_path);
    memcpy(gb->save_path, save_path, sizeof save_path);

    gb_trace_init(&gb->trace);

    cpu_reset(gb);
    timer_reset(gb);
    ppu_reset(gb);
    serial_reset(gb);
    joypad_reset(gb);
    apu_reset(gb);

    /* Power-on I/O values that are not part of the post-boot state. */
    gb->bus.io[0x00] = 0xCF;   /* P1: no group selected, nothing pressed   */
    gb->bus.io[0x0F] = 0xE1;   /* IF: unused bits read 1                   */

    gb->cpu.pc = 0x0100;       /* overridden below if you change it        */

    gb_apply_post_boot_state(gb);
}

/*
 * One instruction, then advance every other component by the same amount of
 * time.
 *
 * M09 note: this function does not change when you move to access-level
 * timing. Memory accesses start ticking 4 T each from inside bus_read /
 * bus_write, and cpu_step() returns only the residual (non-memory) cycles.
 * The call below keeps forwarding that residual, so time is spent exactly once.
 */
u32 gb_step(gb_t *gb)
{
    if (gb->fatal) return 0;

    g_gb_current = gb;

    u16 pc = gb->cpu.pc;
    u32 cycles = cpu_step(gb);

    if (gb->trace.enabled) gb_trace_record(gb, pc, cycles);

    bus_tick(gb, cycles);
    gb->total_ticks += cycles;

    /* The PPU raises frame_ready inside ppu_tick(); lift it to the machine and
     * clear it so the next gb_run_frame() waits for a fresh frame. */
    if (gb->ppu.frame_ready) {
        gb->ppu.frame_ready = false;
        gb->frame_ready = true;
    }
    return cycles;
}

void gb_run_frame(gb_t *gb)
{
    gb->frame_ready = false;
    while (!gb->frame_ready && !gb->fatal) {
        gb_step(gb);
    }
    gb->frame_count++;
}

void gb_serial_byte(gb_t *gb, u8 ch)
{
    if (gb->serial_out) {
        fputc(ch, gb->serial_out);
        fflush(gb->serial_out);
    }
}
