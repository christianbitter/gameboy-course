/*
 * timer.h - DIV / TIMA / TMA / TAC.
 *
 * The STRUCT is provided. The BEHAVIOUR is yours (src/timer.c).
 */
#ifndef GB_TIMER_H
#define GB_TIMER_H

#include "gb/common.h"

typedef struct gb_s gb_t;

typedef struct {
    /*
     * The divider is a free-running 16-bit counter clocked at the MASTER clock:
     * it advances by 1 every T-cycle. DIV (FF04) is its HIGH byte, which is why
     * DIV increments at 4194304 / 256 = 16384 Hz.
     *
     * TIMA is clocked by a selected BIT of this counter, on its falling edge.
     * That is why the rate table in src/timer.c is expressed in bits, and why
     * storing only a DIV byte would make the whole timer wrong.
     */
    u16 div_counter;

    u8 tima;            /* FF05, current counter                        */
    u8 tma;             /* FF06, reload value                           */
    u8 tac;             /* FF07, bit 2 = enable, bits 1-0 = rate select */

    /* M09: the 4-cycle delay between TIMA overflow and the TMA reload, and
     * the fact that TIMA keeps counting during those cycles. */
    u32 reload_delay;
    bool overflow_pending;
} timer_t;

void timer_tick (gb_t *gb, u32 tcycles);
void timer_reset(gb_t *gb);

#endif /* GB_TIMER_H */
