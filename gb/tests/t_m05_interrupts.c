/*
 * t_m05_interrupts.c - milestone 5: interrupts, HALT, timers, serial.
 *
 * This is the milestone that unlocks the test ROMs, so the bar is high:
 * instruction-accurate interrupt dispatch, the EI delay, and a timer whose
 * TIMA is clocked by the falling edge of a bit of the divider.
 */
#include "harness.h"

TEST(m05_interrupt_dispatch)
{
    const u8 prog[] = { 0x00, 0x00 };

    /* A pending, enabled, unmasked interrupt wins over the next instruction. */
    gb_t *gb = t_machine(prog, sizeof prog);
    TEST_ASSERT(gb != NULL, "cart_load() failed");
    gb->cpu.pc = 0x0100;
    gb->cpu.sp = 0xFFFE;
    gb->cpu.ime = true;
    GB_IE(gb) = GB_INT_VBLANK;
    GB_IF(gb) = GB_INT_VBLANK;

    u32 c = cpu_step(gb);
    TEST_EQ(c, 20);                       /* 5 M-cycles */
    TEST_EQ(gb->cpu.pc, 0x0040);          /* VBlank vector */
    TEST_EQ(gb->cpu.ime, 0);              /* IME is cleared by dispatch */
    TEST_EQ(GB_IF(gb) & GB_INT_VBLANK, 0);/* the request is acknowledged */
    TEST_EQ(gb->cpu.sp, 0xFFFC);
    TEST_EQ(bus_read(gb, 0xFFFD), 0x01);  /* pushed return address 0x0100 */
    TEST_EQ(bus_read(gb, 0xFFFC), 0x00);
    t_free(gb);

    /* Priority is by lowest bit number, not by arrival order. */
    gb = t_machine(prog, sizeof prog);
    gb->cpu.pc = 0x0100; gb->cpu.sp = 0xFFFE; gb->cpu.ime = true;
    GB_IE(gb) = GB_INT_VBLANK | GB_INT_TIMER;
    GB_IF(gb) = GB_INT_TIMER;             /* only the timer is pending */
    cpu_step(gb);
    TEST_EQ(gb->cpu.pc, 0x0050);
    t_free(gb);

    gb = t_machine(prog, sizeof prog);
    gb->cpu.pc = 0x0100; gb->cpu.sp = 0xFFFE; gb->cpu.ime = true;
    GB_IE(gb) = GB_INT_VBLANK | GB_INT_TIMER;
    GB_IF(gb) = GB_INT_VBLANK | GB_INT_TIMER;
    cpu_step(gb);
    TEST_EQ(gb->cpu.pc, 0x0040);          /* VBlank wins */
    t_free(gb);

    /* Disabled in IE: the instruction executes and nothing else happens. */
    gb = t_machine(prog, sizeof prog);
    gb->cpu.pc = 0x0100; gb->cpu.sp = 0xFFFE; gb->cpu.ime = true;
    GB_IE(gb) = 0x00;
    GB_IF(gb) = 0x01;
    cpu_step(gb);
    TEST_EQ(gb->cpu.pc, 0x0101);
    TEST_EQ(GB_IF(gb) & 0x01, 0x01);      /* still requested, not acknowledged */
    t_free(gb);
}

TEST(m05_ime_ei_delay)
{
    /* EI enables IME only AFTER the instruction that follows it has run. */
    const u8 prog[] = { 0xFB, 0x00, 0x00 };   /* EI ; NOP ; NOP */
    gb_t *gb = t_machine(prog, sizeof prog);
    TEST_ASSERT(gb != NULL, "cart_load() failed");
    gb->cpu.pc = 0x0100;
    gb->cpu.sp = 0xFFFE;
    GB_IE(gb) = GB_INT_VBLANK;
    GB_IF(gb) = GB_INT_VBLANK;

    cpu_step(gb);                          /* EI */
    TEST_EQ(gb->cpu.pc, 0x0101);
    TEST_ASSERT(!gb->cpu.ime, "IME must still be clear immediately after EI");

    cpu_step(gb);                          /* NOP - the interrupt may not preempt it */
    TEST_EQ(gb->cpu.pc, 0x0102);
    TEST_ASSERT(gb->cpu.ime, "IME must be set once the instruction after EI retires");

    cpu_step(gb);                          /* now the interrupt is taken */
    TEST_EQ(gb->cpu.pc, 0x0040);

    t_free(gb);
}

TEST(m05_halt_wake)
{
    const u8 prog[] = { 0x76, 0x00 };      /* HALT ; NOP */
    gb_t *gb = t_machine(prog, sizeof prog);
    TEST_ASSERT(gb != NULL, "cart_load() failed");
    gb->cpu.pc = 0x0100;
    gb->cpu.sp = 0xFFFE;
    gb->cpu.ime = true;
    GB_IE(gb) = GB_INT_VBLANK;
    GB_IF(gb) = 0x00;

    cpu_step(gb);                          /* HALT: nothing pending, so it halts */
    TEST_EQ(gb->cpu.pc, 0x0101);
    TEST_ASSERT(gb->cpu.halted, "HALT with nothing pending must halt the CPU");

    cpu_step(gb);                          /* still nothing pending */
    TEST_ASSERT(gb->cpu.halted, "the CPU must stay halted while IE & IF == 0");
    TEST_EQ(gb->cpu.pc, 0x0101);

    GB_IF(gb) = GB_INT_VBLANK;             /* a peripheral finally asks */
    cpu_step(gb);
    TEST_ASSERT(!gb->cpu.halted, "a pending enabled interrupt must end the halt");
    TEST_EQ(gb->cpu.pc, 0x0040);

    t_free(gb);
}

TEST(m05_halt_bug)
{
    /*
     * With IME == 0 and an interrupt already pending, HALT does NOT halt: the
     * CPU keeps running and the documented halt bug applies to the next fetch.
     *
     * This test pins only the part every correct implementation must get right
     * (the CPU must not sit halted while IE & IF is non-zero). The precise
     * "byte after HALT is read twice" behaviour is verified by blargg's
     * halt_bug ROM once you can run test ROMs.
     */
    const u8 prog[] = { 0x76, 0x00, 0x00 };
    gb_t *gb = t_machine(prog, sizeof prog);
    TEST_ASSERT(gb != NULL, "cart_load() failed");
    gb->cpu.pc = 0x0100;
    gb->cpu.sp = 0xFFFE;
    gb->cpu.ime = false;                   /* the whole point */
    GB_IE(gb) = GB_INT_VBLANK;
    GB_IF(gb) = GB_INT_VBLANK;             /* pending while IME == 0 */

    cpu_step(gb);
    TEST_ASSERT(!gb->cpu.halted,
                "HALT with IME=0 and a pending interrupt must not halt");

    /* The machine must still make progress afterwards. */
    cpu_step(gb);
    cpu_step(gb);
    TEST_ASSERT(!gb->fatal, "the CPU faulted after the halt bug case");

    t_free(gb);
}

TEST(m05_timer_div_rate)
{
    gb_t *gb = t_machine((const u8[]){ 0x00 }, 1);
    TEST_ASSERT(gb != NULL, "cart_load() failed");

    /* DIV is the high byte of the T-cycle divider: it increments every 256 T. */
    bus_write(gb, 0xFF04, 0x00);           /* writing DIV resets the counter */
    TEST_EQ(bus_read(gb, 0xFF04), 0x00);
    timer_tick(gb, 256);
    TEST_EQ(bus_read(gb, 0xFF04), 0x01);
    timer_tick(gb, 256);
    TEST_EQ(bus_read(gb, 0xFF04), 0x02);

    /* Rate 00 = bit 9 = 4096 Hz -> one TIMA tick every 1024 T-cycles. */
    bus_write(gb, 0xFF04, 0x00);
    bus_write(gb, 0xFF05, 0x00);
    bus_write(gb, 0xFF06, 0x00);
    bus_write(gb, 0xFF07, 0x04);           /* enabled, rate 00 */

    timer_tick(gb, 1023);
    TEST_EQ(bus_read(gb, 0xFF05), 0x00);   /* not yet: the falling edge is the trigger */
    timer_tick(gb, 1);
    TEST_EQ(bus_read(gb, 0xFF05), 0x01);

    /* Disabling TAC stops TIMA but not DIV. */
    bus_write(gb, 0xFF07, 0x00);
    u8 tima_before = bus_read(gb, 0xFF05);
    u8 div_before = bus_read(gb, 0xFF04);
    timer_tick(gb, 4096);
    TEST_EQ(bus_read(gb, 0xFF05), tima_before);
    TEST_ASSERT(bus_read(gb, 0xFF04) != div_before, "DIV must keep running");

    t_free(gb);
}

TEST(m05_timer_tima_overflow)
{
    gb_t *gb = t_machine((const u8[]){ 0x00 }, 1);
    TEST_ASSERT(gb != NULL, "cart_load() failed");

    bus_write(gb, 0xFF04, 0x00);
    bus_write(gb, 0xFF06, 0x42);           /* TMA */
    bus_write(gb, 0xFF05, 0xFF);           /* TIMA: one tick from overflowing */
    bus_write(gb, 0xFF07, 0x05);           /* enabled, rate 01 = bit 3 = 262144 Hz */
    GB_IF(gb) = 0x00;

    timer_tick(gb, 16);                    /* one falling edge */
    TEST_EQ(bus_read(gb, 0xFF05), 0x42);   /* reloaded from TMA */
    TEST_ASSERT(GB_IF(gb) & GB_INT_TIMER, "overflow must raise IF bit 2");

    t_free(gb);
}

TEST(m05_timer_tma_reload)
{
    gb_t *gb = t_machine((const u8[]){ 0x00 }, 1);
    TEST_ASSERT(gb != NULL, "cart_load() failed");

    bus_write(gb, 0xFF04, 0x00);
    bus_write(gb, 0xFF06, 0x42);
    bus_write(gb, 0xFF05, 0xFF);
    bus_write(gb, 0xFF07, 0x05);

    timer_tick(gb, 16);
    TEST_EQ(bus_read(gb, 0xFF05), 0x42);
    timer_tick(gb, 16);
    TEST_EQ(bus_read(gb, 0xFF05), 0x43);   /* and it keeps counting from TMA */

    t_free(gb);
}

TEST(m05_serial_emits_byte)
{
    /*
     * The test ROM protocol: writing SC = 0x81 emits SB on the serial port.
     * Without this, no test ROM can tell you anything.
     */
    gb_t *gb = t_machine((const u8[]){ 0x00 }, 1);
    TEST_ASSERT(gb != NULL, "cart_load() failed");

    FILE *f = tmpfile();
    TEST_ASSERT(f != NULL, "tmpfile() failed");
    gb->serial_out = f;
    GB_IF(gb) = 0x00;

    bus_write(gb, 0xFF01, 'A');            /* SB */
    bus_write(gb, 0xFF02, 0x81);           /* SC: start, internal clock */

    rewind(f);
    int ch = fgetc(f);
    TEST_ASSERT(ch == 'A', "expected 'A' on the serial port, got %d", ch);
    TEST_ASSERT((bus_read(gb, 0xFF02) & 0x80) == 0,
                "SC bit 7 must read as 'transfer finished'");
    TEST_ASSERT(GB_IF(gb) & GB_INT_SERIAL, "the serial interrupt must be requested");

    gb->serial_out = NULL;
    fclose(f);
    t_free(gb);
}

TEST(m05_halt_bug_double_fetch)
{
    /*
     * The other half of the halt bug, and the part m05_halt_bug deliberately leaves
     * to L31: with IME = 0 and an interrupt already pending, HALT does not halt AND
     * the next opcode fetch does not advance PC, so the byte after the HALT is read
     * twice. With a one-byte instruction after HALT that means it EXECUTES twice,
     * which is the observable:
     *
     *   0x0100: 76        HALT
     *   0x0101: 3C        INC A     <- executed twice
     *   0x0102: 00        NOP
     *
     * After three cpu_step calls A must be 2, not 1.
     */
    const u8 prog[] = { 0x76, 0x3C, 0x00 };
    gb_t *gb = t_machine(prog, sizeof prog);
    TEST_ASSERT(gb != NULL, "cart_load() failed");

    gb->cpu.pc = 0x0100;
    gb->cpu.sp = 0xFFFE;
    gb->cpu.ime = false;                  /* the whole point */
    GB_IE(gb) = GB_INT_VBLANK;
    GB_IF(gb) = GB_INT_VBLANK;            /* pending while IME == 0 */

    cpu_step(gb);                         /* HALT: does not halt, arms the bug */
    TEST_ASSERT(!gb->cpu.halted, "HALT must not halt when a request is pending");
    TEST_EQ(gb->cpu.pc, 0x0101);

    cpu_step(gb);                         /* first INC A */
    cpu_step(gb);                         /* the same byte again */

    TEST_ASSERT(gb->cpu.a == 2,
                "the halt bug must make the byte after HALT execute twice, so two "
                "INC A steps give A=2; got A=%u (PC=0x%04X)",
                (unsigned)gb->cpu.a, gb->cpu.pc);
    TEST_EQ(gb->cpu.pc, 0x0102);

    t_free(gb);
}
