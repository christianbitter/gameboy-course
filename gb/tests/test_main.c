/*
 * test_main.c - the test runner and the fixtures. PROVIDED INFRASTRUCTURE.
 */
#include "harness.h"

t_state_t t_state;

/* -------------------------------------------------------------------------- */
/* registration / reporting                                                   */
/* -------------------------------------------------------------------------- */

void t_register(const char *name, void (*fn)(void), const char *todo)
{
    if (t_state.count >= T_MAX_TESTS) return;
    t_state.cases[t_state.count].name = name;
    t_state.cases[t_state.count].fn   = fn;
    t_state.cases[t_state.count].todo = todo;
    t_state.count++;
}

void t_fail(const char *file, int line, const char *fmt, ...)
{
    char detail[512];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(detail, sizeof detail, fmt, ap);
    va_end(ap);

    snprintf(t_state.message, sizeof t_state.message, "%s:%d: %s", file, line, detail);

    if (t_state.running) longjmp(t_state.jump, 1);

    /* Called outside a test: nobody can catch this. */
    fprintf(stderr, "test failure outside a test: %s\n", t_state.message);
    abort();
}

/*
 * GB_UNIMPLEMENTED() routes here under the test runner: record the message and
 * jump back to the runner so the run continues.
 */
static void t_fatal_handler(const char *message)
{
    snprintf(t_state.message, sizeof t_state.message, "%s", message);
    if (t_state.running) longjmp(t_state.jump, 1);
    fprintf(stderr, "%s\n", message);
    abort();
}

void t_install_fatal_handler(void)
{
    g_gb_fatal_handler = t_fatal_handler;
}

static bool name_matches(const char *name, const char *filter)
{
    if (!filter || !*filter) return true;
    return strstr(name, filter) != NULL;
}

int t_run_all(const char *filter)
{
    t_install_fatal_handler();

    for (int i = 0; i < t_state.count; i++) {
        const t_case_t *c = &t_state.cases[i];
        if (!name_matches(c->name, filter)) continue;

        if (c->todo) {
            printf("[  SKIPPED ] %s  (%s)\n", c->name, c->todo);
            t_state.skipped++;
            continue;
        }

        t_state.current = c->name;
        t_state.message[0] = '\0';
        t_state.running = true;

        printf("[ RUN      ] %s\n", c->name);
        fflush(stdout);

        if (setjmp(t_state.jump) == 0) {
            c->fn();
            t_state.running = false;
            printf("[       OK ] %s\n", c->name);
            t_state.passed++;
        } else {
            t_state.running = false;
            printf("[  FAILED  ] %s\n             %s\n", t_state.current, t_state.message);
            t_state.failed++;
        }
        fflush(stdout);
    }

    printf("\n%4d passed, %4d failed, %4d skipped  (%d tests registered)\n",
           t_state.passed, t_state.failed, t_state.skipped, t_state.count);
    if (t_state.failed == 0) {
        printf("ALL GREEN\n");
    } else {
        printf("NOT GREEN YET - that is the point. Work the failures in milestone order.\n");
    }
    return t_state.failed == 0 ? 0 : 1;
}

/* -------------------------------------------------------------------------- */
/* fixtures                                                                   */
/* -------------------------------------------------------------------------- */

void t_build_rom(u8 *rom, size_t size, const u8 *program, size_t program_len)
{
    memset(rom, 0, size);

    /* Minimal valid-ish header. */
    memcpy(rom + 0x0134, "TESTGB", 6);
    rom[0x0143] = 0x00;   /* DMG only                     */
    rom[0x0147] = 0x00;   /* ROM ONLY                     */
    rom[0x0148] = 0x00;   /* 32 KiB                       */
    rom[0x0149] = 0x00;   /* no RAM                       */
    rom[0x014A] = 0x01;   /* overseas                     */

    if (program && program_len) {
        size_t n = program_len;
        if (n > size - 0x0100) n = size - 0x0100;
        memcpy(rom + 0x0100, program, n);
    }

    u8 check = 0;
    for (u16 a = 0x0134; a <= 0x014C; a++) check = (u8)(check - rom[a] - 1);
    rom[0x014D] = check;
}

gb_t *t_load_rom_buffer(const u8 *rom, size_t size)
{
    gb_t *gb = gb_create();
    if (!gb) return NULL;

    if (!cart_load(gb, rom, size, "synthetic.gb")) {
        gb_destroy(gb);
        return NULL;
    }
    gb_reset(gb);
    gb->serial_out = NULL;
    return gb;
}

gb_t *t_machine(const u8 *program, size_t program_len)
{
    u8 *rom = (u8 *)malloc(32768);
    if (!rom) return NULL;
    t_build_rom(rom, 32768, program, program_len);
    gb_t *gb = t_load_rom_buffer(rom, 32768);
    free(rom);
    return gb;
}

u32 t_last_cycles;

gb_t *t_exec(const u8 *bytes, size_t len, const cpu_t *init)
{
    gb_t *gb = t_machine(bytes, len);
    if (!gb) return NULL;

    if (init) gb->cpu = *init;
    gb->cpu.pc = 0x0100;
    gb->cpu.last_opcode = 0;
    gb->cpu.last_opcode_len = 0;
    gb->cpu.instruction_count = 0;

    t_last_cycles = cpu_step(gb);
    return gb;
}

u32 t_exec_n(gb_t *gb, int n)
{
    u32 total = 0;
    for (int i = 0; i < n && !gb->fatal; i++) total += cpu_step(gb);
    t_last_cycles = total;
    return total;
}

void t_free(gb_t *gb)
{
    gb_destroy(gb);
}

/* -------------------------------------------------------------------------- */

int main(int argc, char **argv)
{
    const char *filter = (argc > 1) ? argv[1] : NULL;

    printf("gbemu test runner%s%s%s\n\n",
           filter ? " [filter=" : "", filter ? filter : "", filter ? "]" : "");

    return t_run_all(filter);
}
