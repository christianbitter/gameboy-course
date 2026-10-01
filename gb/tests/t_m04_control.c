/*
 * t_m04_control.c - milestone 4: jumps, the stack, and the CB prefix.
 */
#include "harness.h"

TEST(m04_jp_jr_conditions)
{
    /* JP NZ,a16 taken when Z=0. */
    cpu_t z_clear = t_cpu_init(0x0000, 0, 0, 0, 0xFFFE);
    const u8 jp_nz[] = { 0xC2, 0x34, 0x12 };
    gb_t *g1 = t_exec(jp_nz, sizeof jp_nz, &z_clear);
    TEST_ASSERT(g1 != NULL, "cart_load() failed");
    TEST_EQ(g1->cpu.pc, 0x1234);
    TEST_EQ(t_last_cycles, 16);      /* taken */
    t_free(g1);

    /* ...and not taken when Z=1, in which case it is only 12 T. */
    cpu_t z_set = t_cpu_init(0x8000, 0, 0, 0, 0xFFFE);
    gb_t *g2 = t_exec(jp_nz, sizeof jp_nz, &z_set);
    TEST_ASSERT(g2 != NULL, "cart_load() failed");
    TEST_EQ(g2->cpu.pc, 0x0103);
    TEST_EQ(t_last_cycles, 12);      /* not taken */
    t_free(g2);

    /* JR e8 is relative to the address AFTER the instruction. */
    const u8 jr_back[] = { 0x18, 0xFE };   /* JR -2 -> jumps to itself */
    gb_t *g3 = t_exec(jr_back, sizeof jr_back, NULL);
    TEST_ASSERT(g3 != NULL, "cart_load() failed");
    TEST_EQ(g3->cpu.pc, 0x0100);
    TEST_EQ(t_last_cycles, 12);
    t_free(g3);

    /* JR Z,e8 with Z=1 and offset -2. */
    const u8 jr_z[] = { 0x28, 0xFE };
    gb_t *g4 = t_exec(jr_z, sizeof jr_z, &z_set);
    TEST_ASSERT(g4 != NULL, "cart_load() failed");
    TEST_EQ(g4->cpu.pc, 0x0100);
    TEST_EQ(t_last_cycles, 12);
    t_free(g4);

    /* Same instruction with Z=0: not taken, 8 T, falls through. */
    gb_t *g5 = t_exec(jr_z, sizeof jr_z, &z_clear);
    TEST_ASSERT(g5 != NULL, "cart_load() failed");
    TEST_EQ(g5->cpu.pc, 0x0102);
    TEST_EQ(t_last_cycles, 8);
    t_free(g5);
}

TEST(m04_call_ret_stack)
{
    /*
     * The subroutine must live in the ROM image: writes to 0x0000-0x7FFF are
     * ignored (the MBC snoops them, it does not store them), so you cannot
     * poking a RET into ROM at runtime.
     */
    u8 rom[32768];
    t_build_rom(rom, sizeof rom, (const u8[]){ 0xCD, 0x50, 0x01 }, 3);  /* CALL $0150 */
    rom[0x0150] = 0xC9;                                                /* RET       */

    gb_t *gb = t_load_rom_buffer(rom, sizeof rom);
    TEST_ASSERT(gb != NULL, "cart_load() failed");
    gb->cpu.sp = 0xFFFE;

    cpu_step(gb);                            /* CALL */
    TEST_EQ(gb->cpu.pc, 0x0150);
    TEST_EQ(gb->cpu.sp, 0xFFFC);
    TEST_EQ(t_last_cycles, 24);

    /* The return address is the byte after the 3-byte CALL, little endian on
     * the stack: high byte at SP+1, low byte at SP. */
    TEST_EQ(bus_read(gb, 0xFFFD), 0x01);
    TEST_EQ(bus_read(gb, 0xFFFC), 0x03);

    cpu_step(gb);                            /* RET */
    TEST_EQ(gb->cpu.pc, 0x0103);
    TEST_EQ(gb->cpu.sp, 0xFFFE);
    TEST_EQ(t_last_cycles, 16);

    t_free(gb);
}

TEST(m04_push_pop_order)
{
    const u8 prog[] = { 0xC5, 0xD1 };   /* PUSH BC ; POP DE */
    cpu_t init = t_cpu_init(0, 0x1234, 0, 0, 0xFFFE);
    gb_t *gb = t_exec(prog, sizeof prog, &init);
    TEST_ASSERT(gb != NULL, "cart_load() failed");

    TEST_EQ(gb->cpu.sp, 0xFFFC);
    TEST_EQ(t_last_cycles, 16);
    TEST_EQ(bus_read(gb, 0xFFFD), 0x12);
    TEST_EQ(bus_read(gb, 0xFFFC), 0x34);

    cpu_step(gb);
    TEST_EQ(cpu_de(&gb->cpu), 0x1234);
    TEST_EQ(gb->cpu.sp, 0xFFFE);
    TEST_EQ(t_last_cycles, 12);

    /* POP AF must mask the low nibble of F. */
    bus_write(gb, 0xFFFC, 0xFF);
    bus_write(gb, 0xFFFD, 0xFF);
    const u8 pop_af[] = { 0xF1 };
    gb_t *g2 = t_machine(pop_af, 1);
    TEST_ASSERT(g2 != NULL, "cart_load() failed");
    g2->cpu = t_cpu_init(0, 0, 0, 0, 0xFFFC);
    g2->cpu.pc = 0x0100;
    cpu_step(g2);
    TEST_EQ(g2->cpu.a, 0xFF);
    TEST_EQ(g2->cpu.f, 0xF0);
    t_free(g2);

    t_free(gb);
}

TEST(m04_rst_vectors)
{
    /* RST 08H: push the return address and jump to 0x0008. */
    const u8 prog[] = { 0xCF };
    cpu_t init = t_cpu_init(0, 0, 0, 0, 0xFFFE);
    gb_t *gb = t_exec(prog, sizeof prog, &init);
    TEST_ASSERT(gb != NULL, "cart_load() failed");

    TEST_EQ(gb->cpu.pc, 0x0008);
    TEST_EQ(gb->cpu.sp, 0xFFFC);
    TEST_EQ(t_last_cycles, 16);
    TEST_EQ(bus_read(gb, 0xFFFD), 0x01);
    TEST_EQ(bus_read(gb, 0xFFFC), 0x01);   /* return address 0x0101 */

    /* Every RST opcode at once, for completeness. */
    static const struct { u8 op; u16 target; } rsts[] = {
        { 0xC7, 0x0000 }, { 0xCF, 0x0008 }, { 0xD7, 0x0010 }, { 0xDF, 0x0018 },
        { 0xE7, 0x0020 }, { 0xEF, 0x0028 }, { 0xF7, 0x0030 }, { 0xFF, 0x0038 },
    };
    for (size_t i = 0; i < GB_ARRAY_LEN(rsts); i++) {
        const u8 b[1] = { rsts[i].op };
        gb_t *g = t_exec(b, 1, &init);
        TEST_ASSERT(g != NULL, "cart_load() failed");
        TEST_ASSERT(g->cpu.pc == rsts[i].target,
                    "RST 0x%02X: PC=0x%04X, want 0x%04X",
                    rsts[i].op, g->cpu.pc, rsts[i].target);
        t_free(g);
    }

    t_free(gb);
}

TEST(m04_cb_rotates)
{
    /*
     * The CB page: 0xCB 0x00-0x3F is rotate/shift r8[(op&7)] with the operation
     * chosen by (op>>3)&7, in the order RLC RRC RL RR SLA SRA SWAP SRL.
     * Unlike RLCA/RLA, these DO set Z, and they cost 8 T (register operand).
     */
    static const struct {
        u8 cb; u8 b; u8 f; u8 want_b; u8 want_f; const char *name;
    } cases[] = {
        { 0x00, 0x80, 0x00, 0x01, 0x10, "RLC B 80 -> 01, C" },
        { 0x00, 0x00, 0x00, 0x00, 0x80, "RLC B 00 -> 00, Z set" },
        { 0x08, 0x01, 0x00, 0x80, 0x10, "RRC B 01 -> 80, C" },
        { 0x10, 0x80, 0x00, 0x00, 0x90, "RL B 80,C=0 -> 00, Z C" },
        { 0x18, 0x01, 0x10, 0x80, 0x10, "RR B 01,C=1 -> 80, C" },
        { 0x20, 0x80, 0x00, 0x00, 0x90, "SLA B 80 -> 00, Z C" },
        { 0x28, 0x81, 0x00, 0xC0, 0x10, "SRA B 81 -> C0, C (sign kept)" },
        { 0x30, 0xAB, 0x00, 0xBA, 0x00, "SWAP B AB -> BA, no flags" },
        { 0x38, 0x01, 0x00, 0x00, 0x90, "SRL B 01 -> 00, Z C" },
    };

    for (size_t i = 0; i < GB_ARRAY_LEN(cases); i++) {
        const u8 bytes[2] = { 0xCB, cases[i].cb };
        cpu_t init = t_cpu_init(0, (u16)(cases[i].b << 8), 0, 0, 0xFFFE);
        init.f = (u8)(cases[i].f & 0xF0u);

        gb_t *gb = t_exec(bytes, 2, &init);
        TEST_ASSERT(gb != NULL, "cart_load() failed for %s", cases[i].name);
        TEST_ASSERT(gb->cpu.b == cases[i].want_b && gb->cpu.f == cases[i].want_f,
                    "%s: B=0x%02X (want 0x%02X) F=0x%02X (want 0x%02X)",
                    cases[i].name, gb->cpu.b, cases[i].want_b,
                    gb->cpu.f, cases[i].want_f);
        TEST_EQ(t_last_cycles, 8);
        t_free(gb);
    }
}

TEST(m04_cb_bit_res_set)
{
    static const struct {
        u8 cb; u8 a; u8 f; u8 want_a; u8 want_f; const char *name;
    } cases[] = {
        { 0x7F, 0x80, 0x10, 0x80, 0x30, "BIT 7,A set -> Z clear, H set, C kept" },
        { 0x7F, 0x00, 0x10, 0x00, 0xB0, "BIT 7,A clear -> Z set, H set, C kept" },
        { 0x40, 0x01, 0x00, 0x01, 0x20, "BIT 0,A set" },
        { 0xBF, 0x00, 0x00, 0x80, 0x00, "SET 7,A -> 80, flags untouched" },
        { 0xBF, 0xFF, 0x00, 0xFF, 0x00, "SET 7,A on FF -> FF" },
        { 0x87, 0xFF, 0x00, 0x7F, 0x00, "RES 0,A -> 7F, flags untouched" },
        /* RES/SET on (HL) cost 16 T, not 8. */
    };

    for (size_t i = 0; i < GB_ARRAY_LEN(cases); i++) {
        const u8 bytes[2] = { 0xCB, cases[i].cb };
        cpu_t init = t_cpu_init((u16)(cases[i].a << 8) | (cases[i].f & 0xF0u),
                                0, 0, 0, 0xFFFE);
        gb_t *gb = t_exec(bytes, 2, &init);
        TEST_ASSERT(gb != NULL, "cart_load() failed for %s", cases[i].name);
        TEST_ASSERT(gb->cpu.a == cases[i].want_a && gb->cpu.f == cases[i].want_f,
                    "%s: A=0x%02X (want 0x%02X) F=0x%02X (want 0x%02X)",
                    cases[i].name, gb->cpu.a, cases[i].want_a,
                    gb->cpu.f, cases[i].want_f);
        t_free(gb);
    }

    /* CB on (HL): RES/SET 16 T, BIT 12 T. */
    const u8 res_hl[] = { 0xCB, 0x86 };   /* RES 0,(HL) */
    cpu_t init = t_cpu_init(0, 0, 0, 0xC000, 0xFFFE);
    gb_t *gb = t_machine(res_hl, 2);
    TEST_ASSERT(gb != NULL, "cart_load() failed");
    gb->cpu = init; gb->cpu.pc = 0x0100;
    bus_write(gb, 0xC000, 0xFF);
    cpu_step(gb);
    TEST_EQ(bus_read(gb, 0xC000), 0xFE);
    TEST_EQ(t_last_cycles, 16);
    t_free(gb);

    const u8 bit_hl[] = { 0xCB, 0x7E };   /* BIT 7,(HL) */
    gb_t *g2 = t_machine(bit_hl, 2);
    TEST_ASSERT(g2 != NULL, "cart_load() failed");
    g2->cpu = init; g2->cpu.pc = 0x0100;
    bus_write(g2, 0xC000, 0xFF);
    cpu_step(g2);
    TEST_EQ(g2->cpu.f, 0x20);              /* Z clear because bit 7 is set */
    TEST_EQ(t_last_cycles, 12);
    t_free(g2);
}

TEST(m04_program_loop_sum)
{
    /*
     * A real (if tiny) program - the first one that would work on hardware:
     *
     *   0100: 3E 00        LD A,0
     *   0102: 06 0A        LD B,10
     *   0104: 80           ADD A,B      <- loop
     *   0105: 05           DEC B
     *   0106: 20 FC        JR NZ,-4     (target 0x0104)
     *
     * 10+9+...+1 = 55 = 0x37, and B ends at 0 so the final JR falls through.
     * 32 instructions in total.
     */
    const u8 prog[] = {
        0x3E, 0x00,
        0x06, 0x0A,
        0x80,
        0x05,
        0x20, 0xFC,
    };
    gb_t *gb = t_machine(prog, sizeof prog);
    TEST_ASSERT(gb != NULL, "cart_load() failed");
    gb->cpu.pc = 0x0100;
    gb->cpu.sp = 0xFFFE;

    t_exec_n(gb, 32);

    TEST_ASSERT(!gb->fatal, "the program faulted instead of finishing");
    TEST_EQ(gb->cpu.a, 0x37);
    TEST_EQ(gb->cpu.b, 0x00);
    TEST_EQ(gb->cpu.pc, 0x0108);

    t_free(gb);
}
