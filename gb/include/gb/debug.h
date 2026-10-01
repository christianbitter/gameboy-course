/*
 * debug.h - the instruction tracer, hexdump and disassembler.
 *
 * Provided infrastructure (src/debug.c). You will extend gb_disasm() in M11.
 */
#ifndef GB_DEBUG_H
#define GB_DEBUG_H

#include "gb/common.h"

typedef struct gb_s gb_t;

#define GB_TRACE_DEPTH 256

typedef struct {
    u16 pc;             /* address of the instruction                      */
    u64 ticks;          /* gb->total_ticks before it ran                   */
    u32 cycles;         /* T-cycles it consumed                            */
    u8  op;             /* raw opcode byte                                 */
    u8  len;            /* 1..3, as reported by cpu_step()                 */
    u8  a, f, b, c, d, e, h, l;
    u16 sp;
    char text[32];      /* filled by gb_disasm()                           */
} trace_entry_t;

typedef struct {
    trace_entry_t entries[GB_TRACE_DEPTH];
    u32  head;          /* next write position                             */
    u32  count;         /* entries stored, capped at GB_TRACE_DEPTH        */
    bool enabled;
    FILE *file;         /* optional live trace file                        */
} trace_t;

void gb_trace_init (trace_t *t);
void gb_trace_record(gb_t *gb, u16 pc, u32 cycles);
void gb_trace_print_last(const gb_t *gb, FILE *out, int n);
void gb_trace_dump      (const gb_t *gb, FILE *out);

/*
 * gb_disasm writes a human-readable form of the instruction at `addr` into
 * `out` (at least 32 bytes). The provided implementation prints the raw bytes
 * as hex and never touches the bus. M11 replaces the body with a real
 * disassembler built on your opcode table. Nothing else depends on the text.
 */
void gb_disasm(gb_t *gb, u16 addr, char out[32]);

void gb_hexdump(FILE *out, const u8 *data, size_t len, u16 base);

#endif /* GB_DEBUG_H */
