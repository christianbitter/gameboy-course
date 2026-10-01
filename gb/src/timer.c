/*
 * timer.c - DIV / TIMA / TMA / TAC. YOURS TO IMPLEMENT.
 *
 * Milestone M05: the clean model.
 *   - div_counter is a free-running 16-bit counter clocked so that it advances
 *     1 per 256 T-cycles. DIV (FF04) is its high byte, so DIV increments at
 *     16384 Hz and wraps every 1/64 s. Writing DIV resets the whole counter.
 *   - TAC bit 2 enables TIMA; TAC bits 1-0 pick which bit of div_counter
 *     clocks TIMA:
 *       00 -> bit 9  (4096 Hz)
 *       01 -> bit 3  (262144 Hz)
 *       10 -> bit 5  (65536 Hz)
 *       11 -> bit 7  (16384 Hz)
 *   - On TIMA overflow: TIMA = TMA, and raise IF bit 2 (GB_INT_TIMER).
 *
 * Milestone M09 adds the quarky parts: the 4-cycle delay between overflow and
 * reload (TIMA keeps counting in between), the TMA-reload value quirk, the
 * obscure behaviour when TAC is written on the exact cycle a bit changes, and
 * the DIV-APU relationship. Do not implement those now; making them wrong is
 * worse than not having them.
 *
 * Implementation hint worth ten minutes of thought: ticking TIMA by testing a
 * bit of a counter works, but hardware increments TIMA on the FALLING EDGE of
 * the selected bit. Keep the previous value of that bit around.
 */
#include "gb/gb.h"

void timer_reset(gb_t *gb)
{
    memset(&gb->timer, 0, sizeof gb->timer);
    /* Add anything your implementation needs cleared on reset. */
}

void timer_tick(gb_t *gb, u32 tcycles)
{
    (void)gb; (void)tcycles;
    GB_UNIMPLEMENTED("timer_tick(%u T-cycles) - M05: see the model in this file",
                     tcycles);
}
