/*
 * main.c - the gbemu command line.
 *
 * PROVIDED INFRASTRUCTURE.
 *
 *   gbemu --rom FILE [--frames N] [--max-cycles N]
 *         [--info] [--no-boot-rom] [--boot-rom FILE]
 *         [--serial FILE|-] [--trace FILE]
 *         [--dump-frame out.bmp] [--ppm out.ppm]
 *         [--break-op HEX] [--headless]
 *
 * Exit code 0 on a clean stop, 1 if the machine hit a fatal error, 2 for a
 * usage problem. `--break-op 40` stops when opcode 0x40 (LD B,B) executes and
 * dumps the register file, which is how you script the mooneye-gb suite.
 */
#include "gb/gb.h"

static void usage(const char *argv0)
{
    fprintf(stderr,
        "usage: %s --rom FILE [options]\n"
        "\n"
        "  --rom FILE            cartridge to load (.gb)\n"
        "  --frames N            stop after N frames\n"
        "  --max-cycles N        stop after N T-cycles (decimal or 0x...)\n"
        "  --info                print the parsed cartridge header and exit\n"
        "                        (unless --frames/--max-cycles is also given)\n"
        "  --no-boot-rom         skip the boot ROM (the default without --boot-rom)\n"
        "  --boot-rom FILE       256-byte boot ROM dump to run instead of skipping\n"
        "  --serial FILE|-       write serial output to FILE ('-' = stdout)\n"
        "  --trace FILE          append every executed instruction to FILE\n"
        "  --dump-frame out.bmp  write the last frame as a BMP\n"
        "  --ppm out.ppm         write the last frame as a PPM\n"
        "  --break-op HEX        stop when this opcode executes and dump registers\n"
        "  --headless            no window (the only mode until M11)\n",
        argv0);
}

static void dump_registers(const gb_t *gb, FILE *out)
{
    const cpu_t *c = &gb->cpu;
    fprintf(out,
            "A=%02X F=%02X B=%02X C=%02X D=%02X E=%02X H=%02X L=%02X SP=%04X PC=%04X "
            "IME=%d HALT=%d  ticks=%llu frames=%llu\n",
            c->a, c->f, c->b, c->c, c->d, c->e, c->h, c->l, c->sp, c->pc,
            (int)c->ime, (int)c->halted,
            (unsigned long long)gb->total_ticks, (unsigned long long)gb->frame_count);
}

int main(int argc, char **argv)
{
    const char *rom_path    = NULL;
    const char *serial_path = NULL;
    const char *trace_path  = NULL;
    const char *dump_bmp    = NULL;
    const char *dump_ppm    = NULL;
    const char *boot_path   = NULL;
    bool have_frames = false, have_max = false;
    u64 frames = 0, max_cycles = 0;
    bool want_info = false, skip_boot = false;
    long break_op = -1;

    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        const char *next = (i + 1 < argc) ? argv[i + 1] : NULL;

        if      (!strcmp(a, "--rom")        && next) { rom_path    = argv[++i]; }
        else if (!strcmp(a, "--serial")     && next) { serial_path = argv[++i]; }
        else if (!strcmp(a, "--trace")      && next) { trace_path  = argv[++i]; }
        else if (!strcmp(a, "--dump-frame") && next) { dump_bmp    = argv[++i]; }
        else if (!strcmp(a, "--ppm")        && next) { dump_ppm    = argv[++i]; }
        else if (!strcmp(a, "--boot-rom")   && next) { boot_path   = argv[++i]; }
        else if (!strcmp(a, "--frames")     && next) { frames = strtoull(argv[++i], NULL, 0); have_frames = true; }
        else if (!strcmp(a, "--max-cycles") && next) { max_cycles = strtoull(argv[++i], NULL, 0); have_max = true; }
        else if (!strcmp(a, "--break-op")   && next) { break_op = strtol(argv[++i], NULL, 16); }
        else if (!strcmp(a, "--info"))      { want_info = true; }
        else if (!strcmp(a, "--no-boot-rom")){ skip_boot = true; }
        else if (!strcmp(a, "--headless"))  { /* accepted; the only mode for now */ }
        else if (!strcmp(a, "-h") || !strcmp(a, "--help")) { usage(argv[0]); return 0; }
        else {
            fprintf(stderr, "gbemu: unknown or incomplete option '%s'\n", a);
            usage(argv[0]);
            return 2;
        }
    }

    if (!rom_path) { usage(argv[0]); return 2; }

    gb_t *gb = gb_create();
    if (!gb) { fprintf(stderr, "gbemu: out of memory\n"); return 1; }

    if (boot_path) {
        size_t boot_size = 0;
        u8 *boot = gb_read_file(boot_path, &boot_size);
        if (!boot || boot_size != 0x100) {
            fprintf(stderr, "gbemu: '%s' is not a 256-byte boot ROM\n", boot_path);
            free(boot);
            gb_destroy(gb);
            return 2;
        }
        memcpy(gb->boot_rom, boot, 0x100);
        gb->boot_rom_loaded = true;
        free(boot);
    }

    if (!gb_load_rom_file(gb, rom_path)) { gb_destroy(gb); return 1; }

    gb_reset(gb);
    if (skip_boot || !gb->boot_rom_loaded) gb->boot_rom_enabled = false;

    if (want_info) cart_print_info(gb);

    if (serial_path) {
        if (!strcmp(serial_path, "-")) {
            gb->serial_out = stdout;
        } else {
            gb->serial_out = fopen(serial_path, "wb");
            if (!gb->serial_out) {
                fprintf(stderr, "gbemu: cannot write '%s'\n", serial_path);
                gb_destroy(gb);
                return 2;
            }
        }
    }

    if (trace_path) {
        gb->trace.file = fopen(trace_path, "w");
        if (!gb->trace.file) {
            fprintf(stderr, "gbemu: cannot write '%s'\n", trace_path);
            gb_destroy(gb);
            return 2;
        }
        gb->trace.enabled = true;
    }

    if (want_info && !have_frames && !have_max) {
        gb_destroy(gb);
        return 0;
    }

    if (!have_frames && !have_max) {
        fprintf(stderr, "gbemu: specify --frames N or --max-cycles N "
                        "(there is no window yet; see milestone M11)\n");
        gb_destroy(gb);
        return 2;
    }

    u64 frames_done = 0;
    bool broke = false;

    while (!gb->fatal) {
        if (have_max && gb->total_ticks >= max_cycles) break;
        if (have_frames && frames_done >= frames) break;

        if (break_op >= 0) {
            /* Step one instruction at a time so the breakpoint is exact. */
            gb_step(gb);
            if ((long)gb->cpu.last_opcode == break_op) {
                fprintf(stderr, "\n--- breakpoint: opcode 0x%02lX executed ---\n", break_op);
                dump_registers(gb, stderr);
                gb_trace_print_last(gb, stderr, 8);
                broke = true;
                break;
            }
            if (gb->frame_ready) { gb->frame_ready = false; frames_done++; }
        } else {
            gb_run_frame(gb);
            frames_done++;
        }
    }

    if (gb->fatal) {
        fputs("\n--- machine stopped on a fatal error ---\n", stderr);
        dump_registers(gb, stderr);
        gb_trace_print_last(gb, stderr, 24);
    }

    if (dump_bmp) gb_write_bmp(dump_bmp, ppu_framebuffer(gb));
    if (dump_ppm) gb_write_ppm(dump_ppm, ppu_framebuffer(gb));

    if (gb->cart.ram_dirty && gb->cart.battery) cart_save(gb);

    if (!gb->fatal) {
        fprintf(stderr, "gbemu: %llu frames, %llu T-cycles%s\n",
                (unsigned long long)frames_done,
                (unsigned long long)gb->total_ticks,
                broke ? ", stopped at breakpoint" : "");
    }

    int rc = gb->fatal ? 1 : 0;
    gb_destroy(gb);
    return rc;
}
