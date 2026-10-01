/*
 * t_m10_audio.c - milestone 10 (the APU). WRITE THESE TESTS YOURSELF.
 *
 * Unit-testing timbre is not possible, but a lot of the APU is testable:
 * duty patterns, the frame sequencer period, the LFSR sequence, frequency
 * registers, and whether a channel is silenced by its DAC being off.
 */
#include "harness.h"

TEST_TODO(m10_ch1_square, "write me: duty pattern and the frequency formula for CH1")
TEST_TODO(m10_frame_sequencer_512hz, "write me: 8 steps and which step clocks what")
TEST_TODO(m10_wave_channel, "write me: CH3 plays nibbles from wave RAM")
TEST_TODO(m10_noise_lfsr, "write me: the 15-bit LFSR sequence and the 7-bit mode")
TEST_TODO(m10_volume_envelope, "write me: envelope steps change the channel volume")
