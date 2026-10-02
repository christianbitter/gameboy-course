/*
 * cart.h - cartridge header parsing, MBC banking and save RAM.
 *
 * The STRUCT is provided. The LOGIC is yours (src/cart.c).
 */
#ifndef GB_CART_H
#define GB_CART_H

#include "gb/common.h"

typedef struct gb_s gb_t;

typedef enum {
    MBC_NONE = 0,
    MBC_1,
    MBC_2,
    MBC_3,
    MBC_5,
    MBC_OTHER
} mbc_type_t;

typedef struct {
    u8  *rom;               /* owned buffer, rom_size bytes                */
    size_t rom_size;        /* actual, from the file                      */
    u8  *ram;               /* owned buffer, ram_size bytes (may be NULL)  */
    size_t ram_size;        /* from the header, NOT the file size          */

    mbc_type_t mbc;
    u8  cart_type;          /* header 0x0147                               */
    u8  rom_size_code;      /* header 0x0148                               */
    u8  ram_size_code;      /* header 0x0149                               */
    bool battery;           /* header says RAM is battery-backed           */
    bool has_rtc;
    bool cgb_capable;       /* header 0x0143 bit 7: the ROM knows about CGB.
                             * 0x80 means "CGB-enhanced" (runs on both), 0xC0
                             * means "CGB only" (a DMG must refuse it), 0x00
                             * means DMG-only. Optional stretch, L36. */

    /* MBC latches. Nothing here is stored in ROM; these change which bytes
     * appear at 0x4000-0x7FFF and 0xA000-BFFF. */
    bool ram_enabled;
    u16 rom_bank;           /* current bank at 0x4000                      */
    u8  ram_bank;
    u8  mode;               /* MBC1 only                                   */
    u8  bank_hi;            /* MBC1 secondary 2-bit register               */

    /* MBC3 real-time clock. Kept in the struct rather than in file-static state
     * so that save states (M11) can serialise it. */
    u8   rtc[5];            /* seconds, minutes, hours, days low, days high */
    u8   rtc_latched[5];
    bool rtc_halt;
    bool rtc_latch_armed;

    bool ram_dirty;
    bool header_checksum_ok;
    bool logo_ok;
    char title[17];
} cart_t;

/*
 * Signatures you must implement (src/cart.c):
 *
 * cart_load: takes the whole .gb file, validates loosely, parses the header,
 *            allocates rom/ram, derives <path>.sav into gb->save_path, and loads
 *            it if it already exists. Returns false and prints a reason on a fatal
 *            problem (e.g. the file is too small, or the declared ROM size is
 *            nonsense). Never reject a ROM over the Nintendo logo or the header
 *            checksum - report those instead.
 *
 *            The save path belongs here rather than in the caller, because the save
 *            must be loaded during cart_load, before the ROM runs: a game checks its
 *            own magic bytes on the first read and treats their absence as a new
 *            save. cart_save() then writes gb->save_path.
 */
bool cart_load (gb_t *gb, const u8 *data, size_t size, const char *path);
void cart_unload(gb_t *gb);
u8   cart_read (gb_t *gb, u16 addr);
void cart_write(gb_t *gb, u16 addr, u8 value);
void cart_save (gb_t *gb);          /* write gb->save_path if dirty        */

/* Provided (src/cart.c): the post-load report used by --info, plus the
 * header data tables so you do not have to retype them. */
void        cart_print_info(const gb_t *gb);
const char *cart_mbc_name(u8 cart_type);
u32         cart_rom_size_bytes(u8 rom_size_code);
u32         cart_ram_size_bytes(u8 ram_size_code);

#endif /* GB_CART_H */
