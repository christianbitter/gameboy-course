/*
 * t_m08_mbc.c - cartridge banking and save RAM tests.
 *
 * PROVIDED and real: five tests, no stubs.
 *   L26 (MBC1):  m08_mbc1_bank0_quirk, m08_mbc1_mode_ram
 *   L27 (saves): m08_battery_save_roundtrip
 *   L28 (MBC3/MBC5): m08_mbc3_rtc, m08_mbc5_9bit
 *
 * Two conventions these tests rely on, both stated in cart.h and taught in L26-L28:
 *
 *   1. "A write to 0x0000-0x7FFF is a COMMAND, not a store." The address range
 *      selects which latch is loaded and the byte written is its argument. That is
 *      why the tests write to 0x2000/0x4000/0x6000 and then read 0x4000/0xA000.
 *
 *   2. THE SAVE PATH. `cart_load(gb, data, size, path)` derives `<path>.sav`,
 *      stores it in `gb->save_path`, and loads it if it already exists.
 *      `cart_save(gb)` writes `gb->save_path`. L27 needs this because a fresh
 *      machine must pick up the save *during* cart_load.
 *
 * Every fixture puts a bank's own 16-bit number in the first two bytes of that bank,
 * so reading 0x4000 and 0x4001 tells you exactly which bank is mapped.
 */
#include "harness.h"

#define BANK_SIZE 0x4000

/* A ROM of `size` bytes carrying the given MBC and size codes, with each bank
 * signed. The caller frees it after cart_load has copied it. */
static u8 *build_rom(size_t size, u8 cart_type, u8 rom_code, u8 ram_code)
{
    u8 *rom = (u8 *)malloc(size);
    if (!rom) return NULL;

    t_build_rom(rom, size, NULL, 0);
    rom[0x0147] = cart_type;
    rom[0x0148] = rom_code;
    rom[0x0149] = ram_code;

    u8 check = 0;
    for (u16 a = 0x0134; a <= 0x014C; a++) check = (u8)(check - rom[a] - 1);
    rom[0x014D] = check;

    for (size_t bank = 0; bank < size / BANK_SIZE; bank++) {
        rom[bank * BANK_SIZE]     = (u8)(bank & 0xFF);
        rom[bank * BANK_SIZE + 1] = (u8)((bank >> 8) & 0xFF);
    }
    return rom;
}

static gb_t *load_rom(u8 *rom, size_t size, const char *path)
{
    gb_t *gb = gb_create();
    if (!gb) return NULL;
    if (!cart_load(gb, rom, size, path)) {
        gb_destroy(gb);
        return NULL;
    }
    gb_reset(gb);
    return gb;
}

TEST(m08_mbc1_bank0_quirk)
{
    /* MBC1, 128 KiB = 8 banks. */
    u8 *rom = build_rom(128 * 1024, 0x01, 0x02, 0x00);
    TEST_ASSERT(rom != NULL, "out of memory");
    gb_t *gb = load_rom(rom, 128 * 1024, "gb/build/t_m08_mbc1.gb");
    free(rom);
    TEST_ASSERT(gb != NULL, "cart_load() rejected an MBC1 ROM");

    /* 0x2000-0x3FFF loads the ROM bank register. */
    bus_write(gb, 0x2000, 0x01);
    TEST_EQ(bus_read(gb, 0x4000), 0x01);
    bus_write(gb, 0x2000, 0x03);
    TEST_EQ(bus_read(gb, 0x4000), 0x03);

    /* THE QUIRK: writing 0 selects bank 1, because bank 0 cannot be mapped at
     * 0x4000 on MBC1. A game that relies on this executes garbage without it. */
    bus_write(gb, 0x2000, 0x00);
    TEST_ASSERT(bus_read(gb, 0x4000) == 0x01,
                "writing 0 to the MBC1 ROM bank register must select bank 1; "
                "got bank %u",
                (unsigned)bus_read(gb, 0x4000));

    /* And the bank number is masked to the ROM's bank count: 0x1F -> bank 7. */
    bus_write(gb, 0x2000, 0x1F);
    TEST_ASSERT(bus_read(gb, 0x4000) == 0x07,
                "a 5-bit bank number must be masked to 8 banks; got bank %u",
                (unsigned)bus_read(gb, 0x4000));

    /* Bank 0 is still permanently mapped at 0x0000. */
    TEST_EQ(bus_read(gb, 0x0000), 0x00);

    t_free(gb);
}

TEST(m08_mbc1_mode_ram)
{
    /* MBC1 + RAM + battery, 128 KiB ROM, 32 KiB RAM (four 8 KiB banks). */
    u8 *rom = build_rom(128 * 1024, 0x03, 0x02, 0x03);
    TEST_ASSERT(rom != NULL, "out of memory");
    gb_t *gb = load_rom(rom, 128 * 1024, "gb/build/t_m08_mbc1_mode.gb");
    free(rom);
    TEST_ASSERT(gb != NULL, "cart_load() failed");

    /* 0x0000-0x1FFF is the RAM enable: the low nibble must be 0x0A. */
    bus_write(gb, 0x0000, 0x00);
    bus_write(gb, 0xA000, 0x11);
    TEST_ASSERT(bus_read(gb, 0xA000) != 0x11,
                "with RAM disabled, writes must be dropped and reads must not "
                "return the written value");

    bus_write(gb, 0x0000, 0x0A);
    bus_write(gb, 0x6000, 0x00);      /* mode 0 */
    bus_write(gb, 0x4000, 0x00);      /* secondary 2-bit register = 0 */
    bus_write(gb, 0xA000, 0xAA);
    TEST_EQ(bus_read(gb, 0xA000), 0xAA);

    /* In mode 0 the secondary register affects ROM banking only, so the RAM bank
     * must not move. */
    bus_write(gb, 0x4000, 0x02);
    TEST_ASSERT(bus_read(gb, 0xA000) == 0xAA,
                "mode 0: the 2-bit register must not change the RAM bank; got 0x%02X",
                (unsigned)bus_read(gb, 0xA000));

    /* In mode 1 that same register selects the RAM bank: value 2 -> bank 2. */
    bus_write(gb, 0x6000, 0x01);
    TEST_ASSERT(bus_read(gb, 0xA000) != 0xAA,
                "mode 1: the 2-bit register must select RAM bank 2, which is a "
                "different (empty) bank; got 0x%02X",
                (unsigned)bus_read(gb, 0xA000));
    bus_write(gb, 0xA000, 0xBB);
    TEST_EQ(bus_read(gb, 0xA000), 0xBB);

    /* Back to mode 0 and the original bank must be intact. */
    bus_write(gb, 0x6000, 0x00);
    TEST_ASSERT(bus_read(gb, 0xA000) == 0xAA,
                "mode 0 again: RAM bank 0 must be visible with its old contents; "
                "got 0x%02X",
                (unsigned)bus_read(gb, 0xA000));

    t_free(gb);
}

TEST(m08_mbc3_rtc)
{
    /* MBC3 + timer + RAM + battery, 128 KiB ROM, 32 KiB RAM. */
    u8 *rom = build_rom(128 * 1024, 0x10, 0x02, 0x03);
    TEST_ASSERT(rom != NULL, "out of memory");
    gb_t *gb = load_rom(rom, 128 * 1024, "gb/build/t_m08_mbc3.gb");
    free(rom);
    TEST_ASSERT(gb != NULL, "cart_load() failed");

    bus_write(gb, 0x0000, 0x0A);          /* enable RAM/RTC */

    /* The RTC's live counters are not reachable from the bus, so the test sets
     * them directly - the struct is public, and this is the same technique the PPU
     * tests use for their registers. */
    gb->cart.rtc[0] = 0x42;               /* seconds */
    gb->cart.rtc[1] = 0x13;               /* minutes */

    /* 0x4000-0x5FFF selects RAM bank 0-3 OR RTC register 0x08-0x0C. */
    bus_write(gb, 0x4000, 0x08);
    TEST_ASSERT(bus_read(gb, 0xA000) != 0x42,
                "reads must return the LATCHED RTC registers, so an unlatched live "
                "value must not appear; got 0x%02X",
                (unsigned)bus_read(gb, 0xA000));

    /* Latching takes two writes to 0x6000-0x7FFF: 0x00 then 0x01. */
    bus_write(gb, 0x6000, 0x00);
    bus_write(gb, 0x6000, 0x01);
    TEST_ASSERT(bus_read(gb, 0xA000) == 0x42,
                "after latching, RTC seconds must read 0x42; got 0x%02X",
                (unsigned)bus_read(gb, 0xA000));

    bus_write(gb, 0x4000, 0x09);          /* RTC minutes */
    TEST_ASSERT(bus_read(gb, 0xA000) == 0x13,
                "selecting RTC register 9 must read the minutes snapshot; got 0x%02X",
                (unsigned)bus_read(gb, 0xA000));

    /* The whole point of latching: the live clock moves on, the snapshot does not. */
    gb->cart.rtc[0] = 0x99;
    bus_write(gb, 0x4000, 0x08);
    TEST_ASSERT(bus_read(gb, 0xA000) == 0x42,
                "without a new latch the snapshot must not change; got 0x%02X",
                (unsigned)bus_read(gb, 0xA000));

    /* MBC3's ROM bank register is 7 bits and bank 0 is legal. */
    bus_write(gb, 0x2000, 0x05);
    TEST_ASSERT(bus_read(gb, 0x4000) == 0x05,
                "MBC3 bank register: expected bank 5, got %u",
                (unsigned)bus_read(gb, 0x4000));
    bus_write(gb, 0x2000, 0x00);
    TEST_ASSERT(bus_read(gb, 0x4000) == 0x00,
                "MBC3 allows bank 0 at 0x4000-0x7FFF; got %u",
                (unsigned)bus_read(gb, 0x4000));

    t_free(gb);
}

TEST(m08_mbc5_9bit)
{
    /*
     * 8 MiB = 512 banks. The size matters: with fewer than 257 banks the ninth bit
     * is masked away and this test could not tell the two halves of the bank
     * register apart at all.
     */
    const size_t size = 8u * 1024u * 1024u;
    u8 *rom = build_rom(size, 0x1B, 0x08, 0x03);      /* MBC5 + RAM + battery */
    TEST_ASSERT(rom != NULL, "out of memory");
    gb_t *gb = load_rom(rom, size, "gb/build/t_m08_mbc5.gb");
    free(rom);
    TEST_ASSERT(gb != NULL, "cart_load() failed for an 8 MiB MBC5 ROM");

    bus_write(gb, 0x2000, 0x00);          /* low 8 bits = 0 */
    TEST_ASSERT(bus_read(gb, 0x4000) == 0x00 && bus_read(gb, 0x4001) == 0x00,
                "MBC5 is the one MBC where bank 0 is legal at 0x4000-0x7FFF; "
                "got bank 0x%02X%02X",
                (unsigned)bus_read(gb, 0x4001), (unsigned)bus_read(gb, 0x4000));

    bus_write(gb, 0x2000, 0x05);
    TEST_ASSERT(bus_read(gb, 0x4000) == 0x05 && bus_read(gb, 0x4001) == 0x00,
                "the low 8 bits live at 0x2000-0x2FFF: expected bank 5, got 0x%02X%02X",
                (unsigned)bus_read(gb, 0x4001), (unsigned)bus_read(gb, 0x4000));

    /* The ninth bit is a separate register at 0x3000-0x3FFF. */
    bus_write(gb, 0x3000, 0x01);
    TEST_ASSERT(bus_read(gb, 0x4000) == 0x05 && bus_read(gb, 0x4001) == 0x01,
                "the 9th bank bit lives at 0x3000-0x3FFF: expected bank 0x105, "
                "got 0x%02X%02X",
                (unsigned)bus_read(gb, 0x4001), (unsigned)bus_read(gb, 0x4000));

    bus_write(gb, 0x3000, 0x00);
    TEST_ASSERT(bus_read(gb, 0x4000) == 0x05 && bus_read(gb, 0x4001) == 0x00,
                "clearing the 9th bit must return to bank 5");

    /* RAM banking for MBC5 is a plain 4-bit register at 0x4000-0x5FFF. */
    bus_write(gb, 0x0000, 0x0A);
    bus_write(gb, 0x4000, 0x01);
    bus_write(gb, 0xA000, 0x3C);
    TEST_EQ(bus_read(gb, 0xA000), 0x3C);
    bus_write(gb, 0x4000, 0x00);
    TEST_ASSERT(bus_read(gb, 0xA000) != 0x3C,
                "RAM bank 0 must not contain the byte written to RAM bank 1");

    t_free(gb);
}

TEST(m08_battery_save_roundtrip)
{
    /*
     * The whole point of battery RAM: a byte written in one session is there in the
     * next. gb/build/ is gitignored, and the .sav is removed at the end.
     */
    const char *path = "gb/build/t_m08_roundtrip.gb";

    u8 *rom = build_rom(128 * 1024, 0x03, 0x02, 0x03);   /* MBC1 + RAM + battery */
    TEST_ASSERT(rom != NULL, "out of memory");
    gb_t *gb = load_rom(rom, 128 * 1024, path);
    free(rom);
    TEST_ASSERT(gb != NULL, "cart_load() failed");
    TEST_ASSERT(gb->cart.battery, "cart type 0x03 must set gb->cart.battery");
    TEST_ASSERT(gb->cart.ram_size == 32 * 1024,
                "header RAM size 0x03 is 32 KiB; got %u", (unsigned)gb->cart.ram_size);

    bus_write(gb, 0x0000, 0x0A);          /* enable RAM */
    bus_write(gb, 0xA000, 0x5A);
    bus_write(gb, 0xA123, 0x77);
    TEST_EQ(bus_read(gb, 0xA000), 0x5A);

    cart_save(gb);
    t_free(gb);

    /* A brand-new machine loading the same ROM must find the .sav on its own, which
     * is why cart_load derives the path from its `path` argument. */
    u8 *rom2 = build_rom(128 * 1024, 0x03, 0x02, 0x03);
    TEST_ASSERT(rom2 != NULL, "out of memory");
    gb_t *gb2 = load_rom(rom2, 128 * 1024, path);
    free(rom2);
    TEST_ASSERT(gb2 != NULL, "second cart_load() failed");

    bus_write(gb2, 0x0000, 0x0A);
    TEST_ASSERT(bus_read(gb2, 0xA000) == 0x5A,
                "RAM byte 0 must survive the save/reload round trip; got 0x%02X",
                (unsigned)bus_read(gb2, 0xA000));
    TEST_ASSERT(bus_read(gb2, 0xA123) == 0x77,
                "RAM byte 0x123 must survive the save/reload round trip; got 0x%02X",
                (unsigned)bus_read(gb2, 0xA123));
    t_free(gb2);

    remove("gb/build/t_m08_roundtrip.sav");
}
