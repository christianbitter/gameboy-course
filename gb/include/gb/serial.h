/*
 * serial.h - the link port, and your printf.
 *
 * The STRUCT is provided. The BEHAVIOUR is yours (src/serial.c).
 */
#ifndef GB_SERIAL_H
#define GB_SERIAL_H

#include "gb/common.h"

typedef struct gb_s gb_t;

typedef struct {
    u8  sb;             /* FF01, shift byte                            */
    u8  sc;             /* FF02, bit 7 start/done, bit 1 internal clk  */
    u16 bit_timer;      /* T-cycles until the next bit (512 per bit)    */
    u8  bits_left;
    bool active;
} serial_t;

void serial_tick (gb_t *gb, u32 tcycles);
void serial_reset(gb_t *gb);

/*
 * Provided (src/gb.c): hand one received byte to the host.
 * If gb->serial_out is set, write there; otherwise discard.
 * Blargg's test ROMs print their verdict through this function.
 */
void gb_serial_byte(gb_t *gb, u8 ch);

#endif /* GB_SERIAL_H */
