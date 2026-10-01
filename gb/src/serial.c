/*
 * serial.c - the link port and your printf. YOURS TO IMPLEMENT.
 *
 * Milestone M05. This is the single most valuable 30 lines in the project:
 * every test ROM you will run reports its verdict through this port.
 *
 * The registers:
 *   FF01 SB  the byte being shifted
 *   FF02 SC  bit 7 = transfer in progress / finished, bit 1 = internal clock,
 *            bit 0 = shift clock
 *
 * The shortcut that makes the test ROMs work (docs/04 section 4.5): when
 * software writes SC = 0x81, immediately
 *   1. call gb_serial_byte(gb, gb->serial.sb)   <- prints the character
 *   2. set SC = 0x01                            <- bit 7 clear = transfer done
 *   3. raise IF bit 3 (GB_INT_SERIAL)
 * Test ROMs poll SC bit 7; implementing the real 4096-cycle timing is welcome
 * but not required for them, and a real transfer takes 8 bits * 512 T-cycles.
 *
 * External clock (bit 1 == 0) has no partner in an emulator: the received byte
 * is 0xFF.
 */
#include "gb/gb.h"

void serial_reset(gb_t *gb)
{
    memset(&gb->serial, 0, sizeof gb->serial);
}

void serial_tick(gb_t *gb, u32 tcycles)
{
    (void)gb; (void)tcycles;
    /* TODO(M05): either leave this empty and complete transfers instantly in
     * the FF02 write path, or model the 512 T-cycles per bit here. If you model
     * it, remember to call gb_serial_byte() and raise IF bit 3. */
}
