/*
 * cart.c - header parsing, MBC banking, save RAM. YOURS TO IMPLEMENT.
 *
 * Milestones:
 *   M01  header parse, ROM-only reads, RAM sizing, RAM reads/writes
 *   M08  MBC1/MBC2/MBC3/MBC5 banking registers and .sav persistence
 *
 * Conceptual unlock (docs/03 section 3.5): a write to 0x0000-0x7FFF does not
 * store anything in ROM - the MBC snoops the address range as a command and
 * the written byte as its argument, then changes which 16 KiB window appears.
 */
#include "gb/gb.h"

/* ------------------------------------------------------------------------- */
/* PROVIDED - small data tables and the --info report                        */
/* ------------------------------------------------------------------------- */

const char *cart_mbc_name(u8 cart_type)
{
    switch (cart_type) {
    case 0x00: return "ROM ONLY";
    case 0x01: return "MBC1";
    case 0x02: return "MBC1+RAM";
    case 0x03: return "MBC1+RAM+BATTERY";
    case 0x05: return "MBC2";
    case 0x06: return "MBC2+BATTERY";
    case 0x08: return "ROM+RAM";
    case 0x09: return "ROM+RAM+BATTERY";
    case 0x0B: return "MMM01";
    case 0x0C: return "MMM01+RAM";
    case 0x0D: return "MMM01+RAM+BATTERY";
    case 0x0F: return "MBC3+TIMER+BATTERY";
    case 0x10: return "MBC3+TIMER+RAM+BATTERY";
    case 0x11: return "MBC3";
    case 0x12: return "MBC3+RAM";
    case 0x13: return "MBC3+RAM+BATTERY";
    case 0x19: return "MBC5";
    case 0x1A: return "MBC5+RAM";
    case 0x1B: return "MBC5+RAM+BATTERY";
    case 0x1C: return "MBC5+RUMBLE";
    case 0x1D: return "MBC5+RUMBLE+RAM";
    case 0x1E: return "MBC5+RUMBLE+RAM+BATTERY";
    case 0x20: return "MBC6";
    case 0x22: return "MBC7+SENSOR+RUMBLE+RAM+BATTERY";
    case 0xFC: return "POCKET CAMERA";
    case 0xFD: return "BANDAI TAMA5";
    case 0xFE: return "HuC3";
    case 0xFF: return "HuC1+RAM+BATTERY";
    default:   return "UNKNOWN";
    }
}

u32 cart_rom_size_bytes(u8 code)
{
    if (code <= 0x08) return 32768u << code;   /* 32 KiB .. 8 MiB */
    switch (code) {
    case 0x52: return 1152u * 1024u;
    case 0x53: return 1280u * 1024u;
    case 0x54: return 1536u * 1024u;
    default:   return 0;
    }
}

u32 cart_ram_size_bytes(u8 code)
{
    switch (code) {
    case 0x00: return 0;
    case 0x01: return 2u * 1024u;      /* unused in practice */
    case 0x02: return 8u * 1024u;
    case 0x03: return 32u * 1024u;
    case 0x04: return 128u * 1024u;
    case 0x05: return 64u * 1024u;
    default:   return 0;
    }
}

void cart_print_info(const gb_t *gb)
{
    const cart_t *c = &gb->cart;
    printf("cartridge\n");
    printf("  title            : %s\n", c->title[0] ? c->title : "(none)");
    printf("  type             : 0x%02X %s\n", c->cart_type, cart_mbc_name(c->cart_type));
    printf("  rom              : header 0x%02X -> %u bytes, file %u bytes\n",
           c->rom_size_code, cart_rom_size_bytes(c->rom_size_code),
           (unsigned)c->rom_size);
    printf("  ram              : header 0x%02X -> %u bytes%s\n",
           c->ram_size_code, cart_ram_size_bytes(c->ram_size_code),
           c->battery ? ", battery-backed" : "");
    printf("  header checksum  : %s\n", c->header_checksum_ok ? "ok" : "MISMATCH");
    printf("  nintendo logo    : %s\n", c->logo_ok ? "ok" : "MISMATCH");
    printf("  save file        : %s\n", gb->save_path[0] ? gb->save_path : "(none)");
}

void cart_unload(gb_t *gb)
{
    free(gb->cart.rom);
    free(gb->cart.ram);
    gb->cart.rom = NULL;
    gb->cart.ram = NULL;
    gb->cart.rom_size = 0;
    gb->cart.ram_size = 0;
}

/* ------------------------------------------------------------------------- */
/* TODO(M01 / M08)                                                           */
/* ------------------------------------------------------------------------- */

/*
 * Copy the ROM into gb->cart.rom (you own it; cart_unload frees it), parse the
 * header, set rom_size/ram_size from the HEADER tables above (they are the
 * truth for banking, not the file size), allocate RAM, and load <path>.sav if
 * it exists.
 *
 * Validation policy: warn, never reject. Only fail on things you genuinely
 * cannot continue with - a file too small to hold a header, or a declared ROM
 * size that does not match the file.
 */
bool cart_load(gb_t *gb, const u8 *data, size_t size, const char *path)
{
    (void)gb; (void)data; (void)size; (void)path;
    GB_UNIMPLEMENTED("cart_load() - M01: parse the header and copy the ROM");
    return false;
}

u8 cart_read(gb_t *gb, u16 addr)
{
    (void)gb; (void)addr;
    GB_UNIMPLEMENTED("cart_read(0x%04X) - M01", addr);
    return 0xFF;
}

void cart_write(gb_t *gb, u16 addr, u8 value)
{
    (void)gb; (void)addr; (void)value;
    GB_UNIMPLEMENTED("cart_write(0x%04X, 0x%02X) - M01: RAM writes, M08: MBC commands",
                     addr, value);
}

void cart_save(gb_t *gb)
{
    (void)gb;
    GB_UNIMPLEMENTED("cart_save() - M08: write the .sav");
}
