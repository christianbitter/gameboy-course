/*
 * t_m09_timing.c - milestone 9 (access-level accuracy). WRITE THESE YOURSELF.
 *
 * These are the tests that catch the difference between "runs the games" and
 * "passes mem_timing". They are also the tests most likely to be sensitive to
 * how you restructured bus_tick() - keep the tracer on while you work here.
 */
#include "harness.h"

TEST_TODO(m09_access_cycle_costs, "write me: a 3-byte instruction with an (HL) read costs the documented T")
TEST_TODO(m09_timer_overflow_delay, "write me: the 4-cycle TIMA overflow/reload delay")
TEST_TODO(m09_div_apu_edge, "write me: DIV reset interacts with the timer/APU divider bits")
TEST_TODO(m09_oam_dma_timing, "write me: 160-byte DMA stalls the CPU for 160 M-cycles")
TEST_TODO(m09_stat_blocking, "write me: a STAT source already high does not re-trigger")
