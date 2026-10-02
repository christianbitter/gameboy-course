/*
 * t_m12_stretch.c - the optional CGB stretch (L36). PROVIDED and real: two tests.
 *
 * These are the ONLY two tests in the suite that are gated on a feature the DMG
 * course does not need. Everything else is green when the DMG emulator is done;
 * these two stay red until you decide to do the stretch, and that is intentional:
 * they are a goal you choose, not a debt you inherited.
 *
 * What they cover:
 *   m12_cgb_palettes  - FF68/FF69 (BG palette RAM) and FF6A/FF6B (OBJ palette RAM):
 *                       the 6-bit index, the bit-7 auto-increment, reads through a
 *                       data port, and the fact that neither exists on a DMG.
 *   m12_double_speed  - FF4D (KEY1): arm with bit 0, switch with STOP, read the
 *                       current speed in bit 7 - and the CPU:PPU clock ratio that
 *                       makes double speed mean something.
 *
 * The machine's CGB mode is `gb->cgb`, declared in include/gb/gb.h. The tests set
 * it directly rather than going through the cartridge header, so your detection
 * (header 0x0143 bit 7, recorded as `cart.cgb_capable`) can look however you like.
 */
#include "harness.h"

/* Total dots elapsed in the frame, so a measurement can cross a scanline. */
static u32 ppu_dots(const gb_t *gb)
{
    return (u32)gb->ppu.ly * GB_CYCLES_PER_SCANLINE + gb->ppu.dot;
}

TEST(m12_cgb_palettes)
{
    gb_t *gb = t_machine((const u8[]){ 0x00 }, 1);
    TEST_ASSERT(gb != NULL, "cart_load() failed");

    gb->cgb = true;                       /* the machine's mode, set at load time */
    ppu_reset(gb);

    /* --- the index register: 6 bits of index, bit 7 = auto-increment --- */
    bus_write(gb, 0xFF68, (u8)(0x80 | 0x02));
    TEST_ASSERT((bus_read(gb, 0xFF68) & 0x3F) == 0x02,
                "BCPS must read back the index in bits 0-5; got 0x%02X",
                (unsigned)bus_read(gb, 0xFF68));

    /* --- writing the data port stores at the index and increments it --- */
    bus_write(gb, 0xFF69, 0x1F);
    bus_write(gb, 0xFF69, 0x03);
    TEST_ASSERT((bus_read(gb, 0xFF68) & 0x3F) == 0x04,
                "with auto-increment set, each BCPD write must advance the index by "
                "one, so two writes from index 2 leave it at 4; got %u",
                (unsigned)(bus_read(gb, 0xFF68) & 0x3F));
    TEST_ASSERT(gb->ppu.cgb_bg_ram[2] == 0x1F && gb->ppu.cgb_bg_ram[3] == 0x03,
                "BCPD writes must land in BG palette RAM at the selected index: "
                "[2]=0x%02X [3]=0x%02X, want 0x1F and 0x03",
                (unsigned)gb->ppu.cgb_bg_ram[2], (unsigned)gb->ppu.cgb_bg_ram[3]);

    /* --- reading the data port returns the byte at the current index --- */
    bus_write(gb, 0xFF68, 0x02);
    TEST_ASSERT(bus_read(gb, 0xFF69) == 0x1F,
                "reading BCPD at index 2 must return 0x1F; got 0x%02X",
                (unsigned)bus_read(gb, 0xFF69));
    bus_write(gb, 0xFF68, 0x03);
    TEST_ASSERT(bus_read(gb, 0xFF69) == 0x03,
                "reading BCPD at index 3 must return 0x03; got 0x%02X",
                (unsigned)bus_read(gb, 0xFF69));

    /* --- without bit 7 the index stays put: two writes hit the same byte --- */
    bus_write(gb, 0xFF68, 0x05);
    bus_write(gb, 0xFF69, 0xAA);
    bus_write(gb, 0xFF69, 0xBB);
    TEST_ASSERT(gb->ppu.cgb_bg_ram[5] == 0xBB,
                "with bit 7 clear the index must not move, so the second write "
                "overwrites the first; RAM[5]=0x%02X",
                (unsigned)gb->ppu.cgb_bg_ram[5]);
    TEST_ASSERT((bus_read(gb, 0xFF68) & 0x3F) == 0x05,
                "and the index must still be 5; got %u",
                (unsigned)(bus_read(gb, 0xFF68) & 0x3F));

    /* --- the OBJ palette is a separate 64-byte RAM --- */
    bus_write(gb, 0xFF6A, (u8)(0x80 | 0x00));
    bus_write(gb, 0xFF6B, 0x2C);
    TEST_ASSERT(gb->ppu.cgb_obj_ram[0] == 0x2C,
                "OCPD writes must land in the OBJ palette RAM; [0]=0x%02X",
                (unsigned)gb->ppu.cgb_obj_ram[0]);
    TEST_ASSERT(gb->ppu.cgb_bg_ram[0] != 0x2C,
                "the BG and OBJ palette RAMs must be separate: writing OBJ index 0 "
                "must not touch BG index 0");
    TEST_ASSERT((bus_read(gb, 0xFF6A) & 0x3F) == 0x01,
                "the OBJ index must auto-increment independently of the BG index; "
                "got %u", (unsigned)(bus_read(gb, 0xFF6A) & 0x3F));

    /* --- on a DMG these registers do not exist --- */
    gb->cgb = false;
    bus_write(gb, 0xFF68, 0x80 | 0x07);
    TEST_ASSERT((bus_read(gb, 0xFF68) & 0x3F) != 0x07,
                "FF68 is a CGB-only register: with gb->cgb false it must not behave "
                "as a palette index (a DMG reads 0xFF there, and writes do nothing)");

    t_free(gb);
}

TEST(m12_double_speed)
{
    /*
     * Double speed keeps the CPU and the PPU on separate arithmetic: the CPU clock
     * is doubled, the PPU keeps its own rate, so the same instruction sequence
     * advances the picture by HALF as much. That ratio is the observable, and it is
     * the only reason a game wants the switch.
     *
     * Both runs below execute a 4 T-cycle setup instruction and then 200 NOPs, so
     * the two measurements start from the same place and differ only in speed.
     */
    const int nops = 200;

    u8 prog_normal[1 + 1 + 200];
    u8 prog_double[2 + 1 + 200];
    memset(prog_normal, 0x00, sizeof prog_normal);
    memset(prog_double, 0x00, sizeof prog_double);
    prog_double[0] = 0x10;                 /* STOP */
    prog_double[1] = 0x00;                 /* its unused second byte */

    /* --- run 1: normal speed --- */
    gb_t *gb = t_machine(prog_normal, sizeof prog_normal);
    TEST_ASSERT(gb != NULL, "cart_load() failed");
    gb->cgb = true;
    GB_IE(gb) = 0x00;
    GB_IF(gb) = 0x00;
    gb->cpu.pc = 0x0100;

    gb_step(gb);                           /* the setup instruction, 4 T */
    u32 base = ppu_dots(gb);
    for (int i = 0; i < nops; i++) gb_step(gb);
    u32 normal = ppu_dots(gb) - base;

    TEST_ASSERT(normal > 0, "the PPU did not advance at all in normal speed");

    /* --- run 2: arm KEY1, execute STOP, then the same 200 NOPs --- */
    gb_t *gb2 = t_machine(prog_double, sizeof prog_double);
    TEST_ASSERT(gb2 != NULL, "cart_load() failed");
    gb2->cgb = true;
    GB_IE(gb2) = 0x00;
    GB_IF(gb2) = 0x00;
    gb2->cpu.pc = 0x0100;

    TEST_ASSERT((bus_read(gb2, 0xFF4D) & 0x80) == 0,
                "KEY1 bit 7 must read 0 while the machine is at normal speed");

    /* STOP without arming must NOT switch speed. */
    gb_step(gb2);                          /* STOP, unarmed */
    TEST_ASSERT(gb2->cpu.pc == 0x0102,
                "STOP is a two-byte instruction; PC is 0x%04X", gb2->cpu.pc);
    TEST_ASSERT((bus_read(gb2, 0xFF4D) & 0x80) == 0,
                "an UNARMED STOP must not change the speed; KEY1=0x%02X",
                (unsigned)bus_read(gb2, 0xFF4D));

    bus_write(gb2, 0xFF4D, 0x01);          /* arm the switch */
    TEST_ASSERT((bus_read(gb2, 0xFF4D) & 0x01) != 0,
                "arming the switch must be visible in KEY1 bit 0");
    gb2->cpu.pc = 0x0100;                  /* run the STOP again, now armed */
    gb_step(gb2);

    TEST_ASSERT((bus_read(gb2, 0xFF4D) & 0x80) != 0,
                "an armed STOP must switch to double speed: KEY1=0x%02X",
                (unsigned)bus_read(gb2, 0xFF4D));
    TEST_ASSERT((bus_read(gb2, 0xFF4D) & 0x01) == 0,
                "the arm bit clears itself once the switch has happened");

    base = ppu_dots(gb2);
    for (int i = 0; i < nops; i++) gb_step(gb2);
    u32 doubled = ppu_dots(gb2) - base;

    TEST_ASSERT(doubled > normal * 40 / 100 && doubled < normal * 60 / 100,
                "in double speed the CPU runs twice as fast while the PPU keeps its "
                "own rate, so the same %d NOPs must advance the picture by about "
                "half: %u dots normal vs %u dots doubled",
                nops, (unsigned)normal, (unsigned)doubled);

    /* --- and switching back is the same dance again --- */
    bus_write(gb2, 0xFF4D, 0x01);
    gb2->cpu.pc = 0x0100;
    gb_step(gb2);
    TEST_ASSERT((bus_read(gb2, 0xFF4D) & 0x80) == 0,
                "an armed STOP must return to normal speed: KEY1=0x%02X",
                (unsigned)bus_read(gb2, 0xFF4D));

    /* --- on a DMG, KEY1 does not exist --- */
    gb->cgb = false;
    bus_write(gb, 0xFF4D, 0x01);
    TEST_ASSERT((bus_read(gb, 0xFF4D) & 0x01) == 0,
                "FF4D is a CGB-only register: with gb->cgb false, writing the arm "
                "bit must not make it read back as armed");

    t_free(gb);
    t_free(gb2);
}
