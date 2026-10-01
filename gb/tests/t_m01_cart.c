/*
 * t_m01_cart.c - milestone 1: cartridge header and the address decoder.
 *
 * These tests fail until cart_load(), cart_read() and the bus ranges exist.
 * They are the specification for the M01 deliverable - read them before you
 * write code, not after.
 */
#include "harness.h"

static void make_full_rom(u8 rom[32768])
{
    t_build_rom(rom, 32768, NULL, 0);
    /* Landmarks that exercise different decode paths. */
    rom[0x0000] = 0x11;
    rom[0x0100] = 0x22;
    rom[0x0150] = 0x33;
    rom[0x3FFF] = 0x44;
    rom[0x4000] = 0x55;
    rom[0x7FFF] = 0x66;

    /* Recompute the checksum after editing the header region (0x0134-0x014C
     * is untouched by the writes above, so it is already correct - this is
     * here to show the algorithm the test checks against). */
    u8 check = 0;
    for (u16 a = 0x0134; a <= 0x014C; a++) check = (u8)(check - rom[a] - 1);
    rom[0x014D] = check;
}

TEST(m01_header_parse)
{
    u8 rom[32768];
    make_full_rom(rom);
    gb_t *gb = t_load_rom_buffer(rom, sizeof rom);
    TEST_ASSERT(gb != NULL, "cart_load() rejected a valid 32 KiB ROM");

    TEST_EQ(gb->cart.rom_size, 32768);
    TEST_EQ(gb->cart.cart_type, 0x00);
    TEST_EQ(gb->cart.rom_size_code, 0x00);
    TEST_EQ(gb->cart.ram_size_code, 0x00);
    TEST_EQ(gb->cart.ram_size, 0);
    TEST_ASSERT(strncmp(gb->cart.title, "TESTGB", 6) == 0,
                "title was not parsed from 0x0134: got '%s'", gb->cart.title);
    TEST_ASSERT(gb->cart.mbc == MBC_NONE,
                "cart type 0x00 must map to MBC_NONE, got %d", (int)gb->cart.mbc);

    t_free(gb);
}

TEST(m01_checksum)
{
    u8 rom[32768];
    make_full_rom(rom);

    /* Recompute independently of the emulator. */
    u8 expected = 0;
    for (u16 a = 0x0134; a <= 0x014C; a++) expected = (u8)(expected - rom[a] - 1);

    TEST_EQ(expected, rom[0x014D]);   /* the fixture itself is sane */

    gb_t *gb = t_load_rom_buffer(rom, sizeof rom);
    TEST_ASSERT(gb != NULL, "cart_load() failed");
    TEST_ASSERT(gb->cart.header_checksum_ok,
                "header_checksum_ok must be true: stored 0x%02X, computed 0x%02X",
                rom[0x014D], expected);

    /* A corrupted byte in the checked range must flip the verdict. */
    rom[0x0134] ^= 0xFF;
    gb_t *bad = t_load_rom_buffer(rom, sizeof rom);
    TEST_ASSERT(bad != NULL, "cart_load() must not reject a bad checksum, only report it");
    TEST_ASSERT(!bad->cart.header_checksum_ok, "corrupted header must fail the checksum");

    t_free(bad);
    t_free(gb);
}

TEST(m01_cart_read_rom)
{
    u8 rom[32768];
    make_full_rom(rom);
    gb_t *gb = t_load_rom_buffer(rom, sizeof rom);
    TEST_ASSERT(gb != NULL, "cart_load() failed");

    /* With no boot ROM loaded, 0x0000-0x3FFF is ROM bank 0. */
    TEST_EQ(bus_read(gb, 0x0000), 0x11);
    TEST_EQ(bus_read(gb, 0x0100), 0x22);
    TEST_EQ(bus_read(gb, 0x0150), 0x33);
    TEST_EQ(bus_read(gb, 0x3FFF), 0x44);
    TEST_EQ(bus_read(gb, 0x4000), 0x55);
    TEST_EQ(bus_read(gb, 0x7FFF), 0x66);

    /* 16-bit reads must be two 8-bit accesses, little endian. */
    TEST_EQ(bus_read16(gb, 0x00FF), 0x0000 + (rom[0x00FF]) + (rom[0x0100] << 8));

    t_free(gb);
}

TEST(m01_bus_wram_echo)
{
    gb_t *gb = t_machine((const u8[]){ 0x00 }, 1);
    TEST_ASSERT(gb != NULL, "cart_load() failed");

    bus_write(gb, 0xC123, 0xAB);
    TEST_EQ(bus_read(gb, 0xC123), 0xAB);
    TEST_EQ(bus_read(gb, 0xE123), 0xAB);        /* echo read       */

    bus_write(gb, 0xE456, 0xCD);                /* echo write      */
    TEST_EQ(bus_read(gb, 0xC456), 0xCD);
    TEST_EQ(bus_read(gb, 0xE456), 0xCD);

    /* DFFF and FDFF are the last byte of WRAM and of echo RAM. */
    bus_write(gb, 0xDFFF, 0x5A);
    TEST_EQ(bus_read(gb, 0xFDFF), 0x5A);

    t_free(gb);
}

TEST(m01_bus_unmapped_ff)
{
    gb_t *gb = t_machine((const u8[]){ 0x00 }, 1);
    TEST_ASSERT(gb != NULL, "cart_load() failed");

    /* FEA0-FEFF is prohibited: reads are 0xFF, writes are ignored. */
    TEST_EQ(bus_read(gb, 0xFEA0), 0xFF);
    TEST_EQ(bus_read(gb, 0xFEFF), 0xFF);
    bus_write(gb, 0xFEA0, 0x12);
    TEST_EQ(bus_read(gb, 0xFEA0), 0xFF);

    t_free(gb);
}

TEST(m01_bus_hram_ie)
{
    gb_t *gb = t_machine((const u8[]){ 0x00 }, 1);
    TEST_ASSERT(gb != NULL, "cart_load() failed");

    bus_write(gb, 0xFF80, 0x5A);
    TEST_EQ(bus_read(gb, 0xFF80), 0x5A);
    bus_write(gb, 0xFFFE, 0xA5);
    TEST_EQ(bus_read(gb, 0xFFFE), 0xA5);

    /* FFFF is IE and is not part of HRAM. */
    bus_write(gb, 0xFFFF, 0x1F);
    TEST_EQ(gb->bus.ie, 0x1F);
    TEST_EQ(bus_read(gb, 0xFFFF), 0x1F);

    t_free(gb);
}
