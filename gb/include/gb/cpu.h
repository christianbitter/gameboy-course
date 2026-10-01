/*
 * cpu.h - the LR35902 register file and the CPU entry points.
 *
 * The STRUCT is provided. The BEHAVIOUR is yours (src/cpu.c, src/opcodes.c).
 */
#ifndef GB_CPU_H
#define GB_CPU_H

#include "gb/common.h"

typedef struct gb_s gb_t;

/*
 * Register storage.
 *
 * The eight 8-bit registers are stored individually and the 16-bit pairs are
 * derived with the accessors below. Do NOT add a `union { struct {u8 f,a;};
 * u16 af; }` on top of these fields: byte order inside such a union is
 * implementation-defined and you will get AF reversed.
 */
typedef struct {
    u8  a, f;
    u8  b, c;
    u8  d, e;
    u8  h, l;
    u16 sp;
    u16 pc;

    bool ime;           /* interrupt master enable                        */
    bool ime_pending;   /* EI was executed; enable after the next insn    */
    bool halted;        /* HALT executed, waiting for IE & IF != 0        */
    bool halt_bug;      /* the next fetch does not advance PC             */
    bool stopped;       /* STOP executed                                  */

    u64 instruction_count;
    u32 last_cycles;    /* T-cycles consumed by the last cpu_step()       */
    u8  last_opcode;    /* raw opcode byte just executed                  */
    u8  last_opcode_len;/* 1, 2 or 3 - used by the tracer                 */
} cpu_t;

/* 16-bit pair accessors. Keep the mask: (h << 8) | l alone does not fit u16
 * until you cast, and the compiler is happy to warn about it. */
static inline u16 cpu_af(const cpu_t *c) { return (u16)(((u16)c->a << 8) | (c->f & 0xF0u)); }
static inline u16 cpu_bc(const cpu_t *c) { return (u16)(((u16)c->b << 8) | c->c); }
static inline u16 cpu_de(const cpu_t *c) { return (u16)(((u16)c->d << 8) | c->e); }
static inline u16 cpu_hl(const cpu_t *c) { return (u16)(((u16)c->h << 8) | c->l); }

static inline void cpu_set_af(cpu_t *c, u16 v) { c->a = (u8)(v >> 8); c->f = (u8)(v & 0xF0u); }
static inline void cpu_set_bc(cpu_t *c, u16 v) { c->b = (u8)(v >> 8); c->c = (u8)(v & 0xFFu); }
static inline void cpu_set_de(cpu_t *c, u16 v) { c->d = (u8)(v >> 8); c->e = (u8)(v & 0xFFu); }
static inline void cpu_set_hl(cpu_t *c, u16 v) { c->h = (u8)(v >> 8); c->l = (u8)(v & 0xFFu); }

/* Flag helpers. */
#define CPU_FLAG(c, bit)     (((c)->f & (bit)) != 0)
#define CPU_SET_FLAG(c, bit) ((c)->f |= (u8)(bit))
#define CPU_CLR_FLAG(c, bit) ((c)->f &= (u8)~(bit))
#define CPU_FLAG_SET(c, bit, on) \
    do { if (on) CPU_SET_FLAG((c), (bit)); else CPU_CLR_FLAG((c), (bit)); } while (0)

/*
 * Signatures you must implement (src/cpu.c):
 */
u32  cpu_step(gb_t *gb);          /* execute ONE instruction or dispatch one
                                     interrupt; return T-cycles consumed.

                                     M09 note: once memory accesses tick
                                     themselves (bus_tick(4) inside bus_read /
                                     bus_write), this must return the RESIDUAL
                                     time the CPU still owes: 0 for a normal
                                     instruction, 4 for an internal cycle, 20
                                     for an interrupt dispatch. gb_step()
                                     keeps calling bus_tick(gb, residual), so
                                     nothing in the provided files changes and
                                     no T-cycle is counted twice.        */
void cpu_reset(gb_t *gb);

/*
 * The r8 operand space: index 0..7 = B C D E H L (HL) A.
 *
 * Index 6 is the memory operand: reading it reads bus at HL, writing it writes
 * bus at HL. Implement these two helpers early; they collapse the 0x40-0x7F and
 * 0x80-0xBF blocks into two loops of eight instead of 128 functions.
 */
u8   cpu_read_r8 (gb_t *gb, int idx);
void cpu_write_r8(gb_t *gb, int idx, u8 value);

/* Fetch helpers. Implement in src/cpu.c; they are 2-3 lines each. */
u8   cpu_fetch8 (gb_t *gb);
u16  cpu_fetch16(gb_t *gb);

/*
 * src/opcodes.c: execute one opcode and return the T-cycles it consumed
 * (including the 4 T-cycles for the CB prefix, and the taken/not-taken
 * difference for conditional instructions).
 *
 * If you prefer one giant switch() over a function-pointer table, put the
 * switch in op_execute() - the split between cpu.c and opcodes.c is about
 * keeping fetch/dispatch readable, not about forcing a table design.
 */
u32  op_execute(gb_t *gb, u8 opcode);

/*
 * Guest fault: illegal opcode, or any state the hardware cannot reach.
 * Sets gb->fatal, prints a trace of the last instructions and returns.
 */
void cpu_fatal(gb_t *gb, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));

#endif /* GB_CPU_H */
