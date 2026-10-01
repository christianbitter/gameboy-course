/*
 * debug.c - the instruction tracer and the hexdump.
 *
 * PROVIDED INFRASTRUCTURE. gb_disasm() ships as a placeholder that prints the
 * raw opcode bytes; milestone M11 replaces its body with a real disassembler.
 */
#include "gb/gb.h"

void gb_trace_init(trace_t *t)
{
    memset(t, 0, sizeof *t);
    t->enabled = false;
    t->file = NULL;
}

void gb_trace_record(gb_t *gb, u16 pc, u32 cycles)
{
    trace_t *t = &gb->trace;
    trace_entry_t *e = &t->entries[t->head];

    e->pc        = pc;
    e->ticks     = gb->total_ticks;
    e->cycles    = cycles;
    e->op        = gb->cpu.last_opcode;
    e->len       = gb->cpu.last_opcode_len ? gb->cpu.last_opcode_len : 1;
    e->a         = gb->cpu.a;
    e->f         = gb->cpu.f;
    e->b         = gb->cpu.b;
    e->c         = gb->cpu.c;
    e->d         = gb->cpu.d;
    e->e         = gb->cpu.e;
    e->h         = gb->cpu.h;
    e->l         = gb->cpu.l;
    e->sp        = gb->cpu.sp;

    gb_disasm(gb, pc, e->text);

    t->head = (t->head + 1) % GB_TRACE_DEPTH;
    if (t->count < GB_TRACE_DEPTH) t->count++;

    if (t->file) {
        fprintf(t->file,
                "%08llu  %04X  %-9s  AF=%02X%02X BC=%02X%02X DE=%02X%02X "
                "HL=%02X%02X SP=%04X  %u cyc\n",
                (unsigned long long)e->ticks, e->pc, e->text,
                e->a, e->f, e->b, e->c, e->d, e->e, e->h, e->l, e->sp, e->cycles);
    }
}

static void print_entry(FILE *out, const trace_entry_t *e)
{
    fprintf(out,
            "%08llu  %04X  %-9s  AF=%02X%02X BC=%02X%02X DE=%02X%02X "
            "HL=%02X%02X SP=%04X  %u cyc\n",
            (unsigned long long)e->ticks, e->pc, e->text,
            e->a, e->f, e->b, e->c, e->d, e->e, e->h, e->l, e->sp, e->cycles);
}

void gb_trace_print_last(const gb_t *gb, FILE *out, int n)
{
    const trace_t *t = &gb->trace;
    if (t->count == 0) {
        fputs("(trace empty)\n", out);
        return;
    }
    if (n > (int)t->count) n = (int)t->count;

    u32 start = (t->head + GB_TRACE_DEPTH - (u32)n) % GB_TRACE_DEPTH;
    for (int i = 0; i < n; i++) {
        print_entry(out, &t->entries[(start + (u32)i) % GB_TRACE_DEPTH]);
    }
    fputc('\n', out);
}

void gb_trace_dump(const gb_t *gb, FILE *out)
{
    gb_trace_print_last(gb, out, (int)gb->trace.count);
}

void gb_disasm(gb_t *gb, u16 addr, char out[32])
{
    /*
     * Placeholder. It reads straight out of the cartridge ROM buffer (never
     * through the bus) so it is safe to call from the tracer at any time.
     *
     * M11: replace the body with a lookup in your opcode table and produce
     * real mnemonics such as "CALL $019A" / "LD A,$12" / "JR NZ,-5".
     */
    if (gb->cart.rom && addr < gb->cart.rom_size && addr < 0x8000) {
        u8 b0 = gb->cart.rom[addr];
        u8 b1 = ((size_t)(addr + 1) < gb->cart.rom_size) ? gb->cart.rom[addr + 1] : 0;
        u8 b2 = ((size_t)(addr + 2) < gb->cart.rom_size) ? gb->cart.rom[addr + 2] : 0;
        snprintf(out, 32, "%02X %02X %02X", b0, b1, b2);
    } else {
        snprintf(out, 32, "--");
    }
}

void gb_hexdump(FILE *out, const u8 *data, size_t len, u16 base)
{
    for (size_t i = 0; i < len; i += 16) {
        fprintf(out, "%04X  ", (unsigned)(base + i));
        for (size_t j = 0; j < 16; j++) {
            if (i + j < len) fprintf(out, "%02X ", data[i + j]);
            else             fputs("   ", out);
            if (j == 7) fputc(' ', out);
        }
        fputs(" |", out);
        for (size_t j = 0; j < 16 && i + j < len; j++) {
            u8 c = data[i + j];
            fputc((c >= 32 && c < 127) ? (int)c : '.', out);
        }
        fputs("|\n", out);
    }
}
