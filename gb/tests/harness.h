/*
 * harness.h - the test framework. PROVIDED INFRASTRUCTURE.
 *
 * Design notes worth knowing before you read a failure:
 *
 *  - Tests are auto-registered by the TEST() macro via a GCC constructor, so
 *    you never edit a list to add one.
 *  - The runner installs a fatal handler. GB_UNIMPLEMENTED() therefore marks
 *    just that one test FAILED (with the file:line of the missing behaviour)
 *    and the run continues. A failing run is a progress bar; failures are not
 *    fatal.
 *  - TEST_ASSERT / TEST_EQ longjmp back to the runner, so the rest of that test
 *    is skipped and the failure is reported with expected vs actual.
 *  - cpu_step() is used directly in the CPU tests (not gb_step()) so a missing
 *    bus_tick() in M02 cannot mask a CPU bug.
 */
#ifndef GB_TESTS_HARNESS_H
#define GB_TESTS_HARNESS_H

#include "gb/gb.h"
#include <setjmp.h>
#include <stdarg.h>

#define T_MAX_TESTS 512

typedef struct {
    const char *name;
    void (*fn)(void);
    const char *todo;     /* non-NULL: reported as SKIPPED               */
} t_case_t;

typedef struct {
    t_case_t cases[T_MAX_TESTS];
    int  count;
    int  passed, failed, skipped;
    const char *current;
    char message[1024];
    bool running;
    jmp_buf jump;
} t_state_t;

extern t_state_t t_state;

void t_register(const char *name, void (*fn)(void), const char *todo);
void t_fail(const char *file, int line, const char *fmt, ...)
    __attribute__((format(printf, 3, 4)));
int  t_run_all(const char *filter);
void t_install_fatal_handler(void);

/* ---- test declaration ---------------------------------------------------- */

#define TEST(name)                                                       \
    static void name(void);                                              \
    __attribute__((constructor)) static void t_reg_##name(void)          \
    {                                                                    \
        t_register(#name, name, NULL);                                   \
    }                                                                    \
    static void name(void)

/*
 * TEST_TODO is self-contained: it declares, registers and defines the function
 * as a no-op, so a file can list many of them without bodies. Replace the whole
 * TEST_TODO(...) line with TEST(name) { ... } when you write the real test.
 */
#define TEST_TODO(name, reason)                                          \
    static void name(void);                                              \
    __attribute__((constructor)) static void t_reg_##name(void)          \
    {                                                                    \
        t_register(#name, name, (reason));                               \
    }                                                                    \
    static void name(void) { }

#define TEST_FAIL(...) t_fail(__FILE__, __LINE__, __VA_ARGS__)

#define TEST_ASSERT(cond, ...)                                           \
    do {                                                                 \
        if (!(cond)) { t_fail(__FILE__, __LINE__, __VA_ARGS__); return; }\
    } while (0)

#define TEST_EQ(actual, expected)                                        \
    do {                                                                 \
        long long t_a_ = (long long)(actual);                            \
        long long t_e_ = (long long)(expected);                          \
        if (t_a_ != t_e_) {                                              \
            t_fail(__FILE__, __LINE__,                                   \
                   "expected 0x%llX (%lld), got 0x%llX (%lld)",          \
                   (unsigned long long)t_e_, t_e_,                        \
                   (unsigned long long)t_a_, t_a_);                       \
            return;                                                      \
        }                                                                \
    } while (0)

/* ---- fixtures ------------------------------------------------------------ */

/*
 * Build a 32 KiB ROM image: header fields set, header checksum computed, and
 * `program` copied to 0x0100 where the CPU starts.
 */
void t_build_rom(u8 *rom, size_t size, const u8 *program, size_t program_len);

/* Create a machine and load this exact ROM buffer (copied by cart_load). */
gb_t *t_load_rom_buffer(const u8 *rom, size_t size);

/* Create a machine with a synthetic 32 KiB cart whose program starts at 0x0100. */
gb_t *t_machine(const u8 *program, size_t program_len);

/* Registers with the usual defaults, ready to hand to t_exec(). */
static inline cpu_t t_cpu_init(u16 af, u16 bc, u16 de, u16 hl, u16 sp)
{
    cpu_t c;
    memset(&c, 0, sizeof c);
    cpu_set_af(&c, af);
    cpu_set_bc(&c, bc);
    cpu_set_de(&c, de);
    cpu_set_hl(&c, hl);
    c.sp = sp;
    c.pc = 0x0100;
    return c;
}

/*
 * Load `bytes` at 0x0100, apply `init` (may be NULL), force PC = 0x0100, and
 * execute exactly ONE cpu_step(). Returns the machine; the caller frees it with
 * t_free(). On an unimplemented opcode this longjmps into the runner, so the
 * test fails with the location of the missing code rather than crashing.
 */
gb_t *t_exec(const u8 *bytes, size_t len, const cpu_t *init);

/* T-cycles returned by the most recent t_exec() / t_exec_n() call. */
extern u32 t_last_cycles;

/* Execute N cpu_step() calls; returns the total T-cycles. */
u32 t_exec_n(gb_t *gb, int n);

void t_free(gb_t *gb);

#endif /* GB_TESTS_HARNESS_H */
