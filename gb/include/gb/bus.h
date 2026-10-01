/*
 * bus.h - the address decoder. Everything the CPU touches goes through here.
 *
 * The STRUCT is provided. The DECODE is yours (src/bus.c).
 */
#ifndef GB_BUS_H
#define GB_BUS_H

#include "gb/common.h"

typedef struct gb_s gb_t;

typedef struct {
    u8  vram[0x2000];   /* 8000-9FFF, 8 KiB                              */
    u8  wram[0x2000];   /* C000-DFFF, 8 KiB (echoed at E000-FDFF)        */
    u8  oam[0xA0];      /* FE00-FE9F, 40 sprites x 4 bytes               */
    u8  hram[0x7F];     /* FF80-FFFE, fast internal RAM                  */
    u8  io[0x80];       /* FF00-FF7F, I/O registers                      */
    u8  ie;             /* FFFF, interrupt enable                        */

    /* OAM DMA (FF46). Provided fields; implement the transfer in M01 and the
     * 160-cycle stall in M09. */
    u8  dma_source_high;
    u16 dma_index;
    u32 dma_cycles_left;
    bool dma_active;
} bus_t;

/*
 * Register ownership: every I/O register has exactly one owner.
 *
 *   FF00            joypad_t.select              via joypad_read(gb)
 *   FF01/FF02       serial_t.sb / serial_t.sc
 *   FF04-FF07       timer_t                      (DIV is DERIVED from
 *                                                 timer_t.div_counter)
 *   FF0F  IF        bus.io[0x0F]                 use GB_IF(gb)
 *   FF40-FF4B       ppu_t fields                 (LY is read-only)
 *   FF46  DMA       bus_t dma_* fields
 *   FF50            gb->boot_rom_enabled
 *   FFFF  IE        bus.ie                       use GB_IE(gb)
 *   everything else bus.io[]
 *
 * bus_read()/bus_write() are the only code that may touch a component
 * register: they translate an address into the owner's field and back.
 */
#define GB_IF(gb) ((gb)->bus.io[0x0F])
#define GB_IE(gb) ((gb)->bus.ie)

/*
 * Signatures you must implement (src/bus.c):
 */
u8   bus_read   (gb_t *gb, u16 addr);
void bus_write  (gb_t *gb, u16 addr, u8 value);
u16  bus_read16 (gb_t *gb, u16 addr);
void bus_write16(gb_t *gb, u16 addr, u16 value);
void bus_tick   (gb_t *gb, u32 tcycles);

/*
 * Implemented for you (src/gb.c): raises the requested interrupt bits in IF.
 * `bits` is a mask built from GB_INT_*.
 */
void bus_request_interrupt(gb_t *gb, u8 bits);

#endif /* GB_BUS_H */
