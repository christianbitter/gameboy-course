/*
 * t_m03_alu.c - milestone 3: the load matrix, the 8-bit ALU, INC/DEC, rotates.
 *
 * These are data-driven on purpose. If you write the flags by hand per family
 * and this table disagrees with you, the table is right: it was derived from
 * the rules in docs/02-cpu.md section 2.4, where every cell is explained.
 *
 * Read the "want F" column carefully - H and C are where the days disappear.
 */
#include "harness.h"

typedef struct {
    u8 op;
    u8 a, b, f;          /* inputs: A, B, incoming flags              */
    u8 want_a, want_f;   /* expected A and F after the instruction    */
    const char *name;
} alu_case_t;

/*
 * OP A,B - result lands in A.
 *
 * The immediate ALU forms are 0xC6/0xCE/0xD6/0xDE/0xE6/0xEE/0xF6/0xFE, i.e.
 * exactly the opcodes where (op & 0xC7) == 0xC6. Those take their operand from
 * the byte AFTER the opcode, so for them the program is two bytes long and `b`
 * plays the role of the immediate. Getting this length wrong makes a CORRECT
 * implementation fail, which is the worst possible bug in a test fixture.
 */
static void run_a_case(const alu_case_t *tc)
{
    cpu_t init = t_cpu_init((u16)((tc->a << 8) | (tc->f & 0xF0u)),
                            (u16)(tc->b << 8), 0, 0, 0xFFFE);
    const bool immediate = ((tc->op & 0xC7u) == 0xC6u);
    const u8 bytes[2] = { tc->op, tc->b };
    gb_t *gb = t_exec(bytes, immediate ? 2 : 1, &init);
    TEST_ASSERT(gb != NULL, "cart_load() failed for %s", tc->name);
    TEST_ASSERT(gb->cpu.a == tc->want_a && gb->cpu.f == tc->want_f,
                "%s: A=0x%02X (want 0x%02X)  F=0x%02X (want 0x%02X)",
                tc->name, gb->cpu.a, tc->want_a, gb->cpu.f, tc->want_f);
    t_free(gb);
}

/* INC/DEC B: result lands in B, and C must survive. */
static void run_b_case(const alu_case_t *tc)
{
    cpu_t init = t_cpu_init(0x0000,
                            (u16)(tc->b << 8),
                            0, 0, 0xFFFE);
    init.f = (u8)(tc->f & 0xF0u);
    const u8 bytes[1] = { tc->op };
    gb_t *gb = t_exec(bytes, 1, &init);
    TEST_ASSERT(gb != NULL, "cart_load() failed for %s", tc->name);
    TEST_ASSERT(gb->cpu.b == tc->want_a && gb->cpu.f == tc->want_f,
                "%s: B=0x%02X (want 0x%02X)  F=0x%02X (want 0x%02X)",
                tc->name, gb->cpu.b, tc->want_a, gb->cpu.f, tc->want_f);
    t_free(gb);
}

TEST(m03_alu_add_flags)
{
    static const alu_case_t cases[] = {
        { 0x80, 0x0F, 0x01, 0x00, 0x10, 0x20, "ADD A,B 0F+01 -> H" },
        { 0x80, 0x00, 0x00, 0x00, 0x00, 0x80, "ADD A,B 00+00 -> Z" },
        { 0x80, 0xFF, 0x01, 0x00, 0x00, 0xB0, "ADD A,B FF+01 -> ZHC" },
        { 0x80, 0x3A, 0xC6, 0x00, 0x00, 0xB0, "ADD A,B 3A+C6 -> ZHC" },
        { 0x80, 0x12, 0x34, 0x00, 0x46, 0x00, "ADD A,B 12+34 -> none" },
        { 0xC6, 0x0F, 0x01, 0x00, 0x10, 0x20, "ADD A,d8 immediate form" },
    };
    for (size_t i = 0; i < GB_ARRAY_LEN(cases); i++) run_a_case(&cases[i]);
}

/* Split by flag family so that each lesson gates on its own rule:
 * ADD/ADC is L08 (carry out), SUB/SBC/CP is L09 (borrow). */
TEST(m03_alu_adc_flags)
{
    static const alu_case_t cases[] = {
        { 0x88, 0x0F, 0x00, 0x10, 0x10, 0x20, "ADC A,B 0F+00+C -> H, C cleared" },
        { 0x88, 0xFF, 0x00, 0x10, 0x00, 0xB0, "ADC A,B FF+00+C -> ZHC" },
        { 0x88, 0x10, 0x20, 0x10, 0x31, 0x00, "ADC A,B 10+20+C -> none" },
        { 0xCE, 0x0F, 0x01, 0x10, 0x11, 0x20, "ADC A,d8 immediate form" },
    };
    for (size_t i = 0; i < GB_ARRAY_LEN(cases); i++) run_a_case(&cases[i]);
}

TEST(m03_alu_sbc_flags)
{
    static const alu_case_t cases[] = {
        { 0x98, 0x10, 0x01, 0x10, 0x0E, 0x60, "SBC A,B 10-01-C -> H only" },
        { 0x98, 0x00, 0x00, 0x10, 0xFF, 0x70, "SBC A,B 00-00-C -> NHC" },
        { 0x98, 0x05, 0x02, 0x00, 0x03, 0x40, "SBC A,B 05-02 -> N" },
        { 0xDE, 0x10, 0x01, 0x10, 0x0E, 0x60, "SBC A,d8 immediate form" },
    };
    for (size_t i = 0; i < GB_ARRAY_LEN(cases); i++) run_a_case(&cases[i]);
}

TEST(m03_alu_sub_flags)
{
    static const alu_case_t cases[] = {
        { 0x90, 0x10, 0x01, 0x00, 0x0F, 0x60, "SUB B 10-01 -> NH" },
        { 0x90, 0x00, 0x01, 0x00, 0xFF, 0x70, "SUB B 00-01 -> NHC" },
        { 0x90, 0x05, 0x05, 0x00, 0x00, 0xC0, "SUB B 05-05 -> ZN" },
        { 0x90, 0x88, 0x11, 0x00, 0x77, 0x40, "SUB B 88-11 -> N only" },
        { 0xD6, 0x10, 0x01, 0x00, 0x0F, 0x60, "SUB d8 immediate form" },
    };
    for (size_t i = 0; i < GB_ARRAY_LEN(cases); i++) run_a_case(&cases[i]);
}

TEST(m03_alu_logic_flags)
{
    static const alu_case_t cases[] = {
        /* AND sets H and clears C. It is the odd one out. */
        { 0xA0, 0xF0, 0x0F, 0x00, 0x00, 0xA0, "AND B F0&0F -> ZH" },
        { 0xA0, 0xFF, 0x0F, 0x00, 0x0F, 0x20, "AND B FF&0F -> H only" },
        { 0xA0, 0xF0, 0x70, 0x10, 0x70, 0x20, "AND B must CLEAR incoming C" },
        /* XOR / OR clear H and C. */
        { 0xA8, 0xFF, 0xFF, 0x00, 0x00, 0x80, "XOR B FF^FF -> Z" },
        { 0xA8, 0x0F, 0xF0, 0x10, 0xFF, 0x00, "XOR B clears H and C" },
        { 0xB0, 0x0F, 0xF0, 0x00, 0xFF, 0x00, "OR B 0F|F0 -> none" },
        { 0xB0, 0x00, 0x00, 0x10, 0x00, 0x80, "OR B 00|00 -> Z, C cleared" },
    };
    for (size_t i = 0; i < GB_ARRAY_LEN(cases); i++) run_a_case(&cases[i]);
}

TEST(m03_alu_cp_no_write)
{
    static const alu_case_t cases[] = {
        { 0xB8, 0x10, 0x01, 0x00, 0x10, 0x60, "CP B 10 vs 01 -> A unchanged, NH" },
        { 0xB8, 0x05, 0x05, 0x00, 0x05, 0xC0, "CP B equal -> ZN, A unchanged" },
        { 0xB8, 0x00, 0x01, 0x00, 0x00, 0x70, "CP B 00 vs 01 -> NHC, A unchanged" },
        { 0xFE, 0x10, 0x01, 0x00, 0x10, 0x60, "CP d8 immediate form" },
    };
    for (size_t i = 0; i < GB_ARRAY_LEN(cases); i++) run_a_case(&cases[i]);
}

TEST(m03_inc_dec_flags)
{
    static const alu_case_t cases[] = {
        /* INC/DEC must not touch C. The incoming F has C set to prove it. */
        { 0x04, 0x10, 0x0F, 0x10, 0x10, 0x30, "INC B 0F -> 10, H set, C kept" },
        { 0x04, 0x00, 0xFF, 0x00, 0x00, 0xA0, "INC B FF -> 00, ZH" },
        { 0x04, 0x00, 0x00, 0x10, 0x01, 0x10, "INC B 00 -> 01, C kept" },
        { 0x05, 0x00, 0x01, 0x10, 0x00, 0xD0, "DEC B 01 -> 00, ZN, C kept" },
        { 0x05, 0x00, 0x10, 0x00, 0x0F, 0x60, "DEC B 10 -> 0F, N H" },
        { 0x05, 0x00, 0x00, 0x00, 0xFF, 0x60, "DEC B 00 -> FF, N H" },
    };
    for (size_t i = 0; i < GB_ARRAY_LEN(cases); i++) run_b_case(&cases[i]);
}

TEST(m03_daa)
{
    /*
     * DAA corrects BCD arithmetic using the N flag. The high-nibble
     * correction is 0x60, not 0x06 - see docs/02-cpu.md section 2.4.
     */
    static const alu_case_t cases[] = {
        { 0x27, 0x9A, 0x00, 0x00, 0x00, 0x90, "DAA 9A -> 00 with carry" },
        { 0x27, 0x3C, 0x00, 0x00, 0x42, 0x00, "DAA 3C -> 42" },
        { 0x27, 0x2D, 0x00, 0x60, 0x27, 0x40, "DAA 2D after SUB -> 27" },
        { 0x27, 0xFF, 0x00, 0x70, 0x99, 0x50, "DAA FF after SUB with carry -> 99" },
        { 0x27, 0x00, 0x00, 0x20, 0x06, 0x00, "DAA 00 with H -> 06" },
    };
    for (size_t i = 0; i < GB_ARRAY_LEN(cases); i++) run_a_case(&cases[i]);
}

TEST(m03_rotates_a)
{
    static const alu_case_t cases[] = {
        { 0x07, 0x85, 0x00, 0x00, 0x0B, 0x10, "RLCA 85 -> 0B, C set" },
        { 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, "RLCA 00 -> 00, Z stays CLEAR" },
        { 0x0F, 0x85, 0x00, 0x00, 0xC2, 0x10, "RRCA 85 -> C2, C set" },
        { 0x0F, 0x01, 0x00, 0x00, 0x80, 0x10, "RRCA 01 -> 80, C set" },
        { 0x17, 0x85, 0x00, 0x00, 0x0A, 0x10, "RLA 85,C=0 -> 0A, C set" },
        { 0x17, 0x00, 0x00, 0x10, 0x01, 0x00, "RLA 00,C=1 -> 01, C clear" },
        { 0x1F, 0x85, 0x00, 0x10, 0xC2, 0x10, "RRA 85,C=1 -> C2, C set" },
        { 0x1F, 0x00, 0x00, 0x00, 0x00, 0x00, "RRA 00,C=0 -> 00, Z stays CLEAR" },
        { 0x2F, 0x35, 0x00, 0x90, 0xCA, 0xF0, "CPL 35 -> CA, N H, Z and C kept" },
        { 0x37, 0x00, 0x00, 0x80, 0x00, 0x90, "SCF -> C set, Z kept, N H clear" },
        { 0x3F, 0x00, 0x00, 0x90, 0x00, 0x80, "CCF C=1 -> C=0, Z kept" },
        { 0x3F, 0x00, 0x00, 0x80, 0x00, 0x90, "CCF C=0 -> C=1" },
    };
    for (size_t i = 0; i < GB_ARRAY_LEN(cases); i++) run_a_case(&cases[i]);
}

static u8 reg_by_index(const cpu_t *c, int idx)
{
    switch (idx) {
    case 0: return c->b;
    case 1: return c->c;
    case 2: return c->d;
    case 3: return c->e;
    case 4: return c->h;
    case 5: return c->l;
    case 6: return 0;        /* (HL) is read from memory, not from the struct */
    default: return c->a;
    }
}

static const char *reg_name(int idx)
{
    static const char *names[8] = { "B", "C", "D", "E", "H", "L", "(HL)", "A" };
    return names[idx & 7];
}

TEST(m03_ld_r_r_matrix)
{
    /*
     * 0x40-0x7F is LD dst,src with dst=(op>>3)&7 and src=op&7 over the operand
     * space B C D E H L (HL) A. That single rule is 64 opcodes - including the
     * 0x76 slot which is HALT, i.e. LD (HL),(HL).
     *
     * When the memory operand is involved, HL must be a valid address; the
     * fixture uses 0xC000 and puts 0x77 there.
     */
    for (int dst = 0; dst < 8; dst++) {
        for (int src = 0; src < 8; src++) {
            if (dst == 6 && src == 6) continue;   /* that is HALT */

            cpu_t init = t_cpu_init(0x8800, 0x1100, 0x3300, 0, 0xFFFE);
            init.c = 0x22;
            init.e = 0x44;
            if (dst == 6 || src == 6) { init.h = 0xC0; init.l = 0x00; }
            else                      { init.h = 0x55; init.l = 0x66; }

            const u8 bytes[1] = { (u8)(0x40 | (dst << 3) | src) };
            gb_t *gb = t_machine(bytes, 1);
            TEST_ASSERT(gb != NULL, "cart_load() failed");

            gb->cpu = init;
            gb->cpu.pc = 0x0100;
            if (dst == 6 || src == 6) bus_write(gb, 0xC000, 0x77);

            cpu_step(gb);

            u8 want = (src == 6) ? 0x77 : reg_by_index(&init, src);
            u8 got  = (dst == 6) ? bus_read(gb, 0xC000) : reg_by_index(&gb->cpu, dst);

            TEST_ASSERT(got == want,
                        "LD %s,%s (op 0x%02X): got 0x%02X, want 0x%02X",
                        reg_name(dst), reg_name(src), bytes[0], got, want);
            t_free(gb);
        }
    }
}

TEST(m03_hl_indirect_cycles)
{
    /* The (HL) operand costs 4 extra T-cycles. Getting this wrong is invisible
     * until timing tests, which is exactly why it is tested here. */
    const u8 inc_hl[] = { 0x34 };                 /* INC (HL)    */
    const u8 ld_hl_d8[] = { 0x36, 0x09 };         /* LD (HL),d8  */
    const u8 ld_a_hl[] = { 0x7E };                /* LD A,(HL)   */
    const u8 ld_hl_b[] = { 0x70 };                /* LD (HL),B   */
    const u8 add_a_hl[] = { 0x86 };               /* ADD A,(HL)  */

    cpu_t init = t_cpu_init(0x0000, 0x4200, 0, 0xC000, 0xFFFE);

    gb_t *g1 = t_machine(inc_hl, 1);
    TEST_ASSERT(g1 != NULL, "cart_load() failed");
    g1->cpu = init; g1->cpu.pc = 0x0100;
    bus_write(g1, 0xC000, 0x05);
    cpu_step(g1);
    TEST_EQ(bus_read(g1, 0xC000), 0x06);
    TEST_EQ(t_last_cycles, 12);
    t_free(g1);

    gb_t *g2 = t_machine(ld_hl_d8, 2);
    TEST_ASSERT(g2 != NULL, "cart_load() failed");
    g2->cpu = init; g2->cpu.pc = 0x0100;
    cpu_step(g2);
    TEST_EQ(bus_read(g2, 0xC000), 0x09);
    TEST_EQ(t_last_cycles, 12);
    t_free(g2);

    gb_t *g3 = t_machine(ld_a_hl, 1);
    TEST_ASSERT(g3 != NULL, "cart_load() failed");
    g3->cpu = init; g3->cpu.pc = 0x0100;
    bus_write(g3, 0xC000, 0x33);
    cpu_step(g3);
    TEST_EQ(g3->cpu.a, 0x33);
    TEST_EQ(t_last_cycles, 8);
    t_free(g3);

    gb_t *g4 = t_machine(ld_hl_b, 1);
    TEST_ASSERT(g4 != NULL, "cart_load() failed");
    g4->cpu = init; g4->cpu.pc = 0x0100;
    cpu_step(g4);
    TEST_EQ(bus_read(g4, 0xC000), 0x42);
    TEST_EQ(t_last_cycles, 8);
    t_free(g4);

    gb_t *g5 = t_machine(add_a_hl, 1);
    TEST_ASSERT(g5 != NULL, "cart_load() failed");
    g5->cpu = init; g5->cpu.pc = 0x0100;
    bus_write(g5, 0xC000, 0x01);
    cpu_step(g5);
    TEST_EQ(g5->cpu.a, 0x01);
    TEST_EQ(t_last_cycles, 8);
    t_free(g5);
}
