/*
 * bus.c - the address decoder. YOURS TO IMPLEMENT.
 *
 * Milestones:
 *   M01  ROM-only map, WRAM, echo RAM, HRAM, IE, unmapped = 0xFF, OAM DMA copy
 *   M05  IF/IE plumbing, serial and timer register dispatch
 *   M07  PPU register dispatch, mode-3 access restrictions
 *   M09  access-level cycle costs and the OAM DMA stall
 *
 * The decode itself is a range chain. Move the rule of thumb from
 * docs/03-memory-and-cartridge.md:
 *   - an address has exactly one owner, and bus.c is the only place that knows
 *     which one (see the ownership table in include/gb/bus.h);
 *   - every read goes through the owner's read path, so side effects happen;
 *   - 16-bit accesses are two 8-bit accesses.
 */
#include "gb/gb.h"

/* ------------------------------------------------------------------------- */
/* PROVIDED                                                                  */
/* ------------------------------------------------------------------------- */

void bus_request_interrupt(gb_t *gb, u8 bits)
{
    GB_IF(gb) = (u8)((GB_IF(gb) | (bits & GB_INT_MASK)) | 0xE0u);
}

/* ------------------------------------------------------------------------- */
/* TODO(M01) - the decoder                                                   */
/* ------------------------------------------------------------------------- */

u8 bus_read(gb_t *gb, u16 addr)
{
    (void)gb; (void)addr;
    GB_UNIMPLEMENTED("bus_read(0x%04X) - M01", addr);
    return 0xFF;
}

void bus_write(gb_t *gb, u16 addr, u8 value)
{
    (void)gb; (void)addr; (void)value;
    GB_UNIMPLEMENTED("bus_write(0x%04X, 0x%02X) - M01", addr, value);
}

u16 bus_read16(gb_t *gb, u16 addr)
{
    (void)gb; (void)addr;
    GB_UNIMPLEMENTED("bus_read16(0x%04X) - M01: two 8-bit reads, low then high", addr);
    return 0xFFFF;
}

void bus_write16(gb_t *gb, u16 addr, u16 value)
{
    (void)gb; (void)addr; (void)value;
    GB_UNIMPLEMENTED("bus_write16(0x%04X) - M01: two 8-bit writes, low then high", addr);
}

/*
 * Advance every component by the same number of T-cycles. This is the seam
 * that lets you move from instruction-stepped to access-stepped execution in
 * M09 without touching the CPU: keep the call here, and later add extra
 * bus_tick(4) calls inside bus_read/bus_write - but then subtract the
 * instruction total so you do not count time twice.
 */
void bus_tick(gb_t *gb, u32 tcycles)
{
    (void)gb; (void)tcycles;
    GB_UNIMPLEMENTED("bus_tick(%u) - M05: timer_tick + ppu_tick + serial_tick", tcycles);
}
