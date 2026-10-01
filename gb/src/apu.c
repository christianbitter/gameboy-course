/*
 * apu.c - the four-channel sound unit. YOURS TO IMPLEMENT.
 *
 * Milestone M10. See docs/05-audio.md for the full register map and the
 * channel models. The short version:
 *
 *   - CH1 square + sweep, CH2 square, CH3 wave RAM, CH4 noise (15-bit LFSR).
 *   - A 512 Hz frame sequencer with 8 steps clocks length (256 Hz), envelope,
 *     and sweep. Getting the sequencer wrong is what makes sound "buzzy".
 *   - Square frequency from an 11-bit value X: f = 131072 / (2048 - X) Hz.
 *   - Produce interleaved stereo s16 at GB_APU_SAMPLE_RATE, filling
 *     gb->apu.buffer and updating buffer_len. The host drains it with
 *     apu_read_samples().
 *
 * Order that keeps you sane: master power + NR50/NR51 -> CH1 square with
 * duty and fixed volume -> CH2 -> CH3 wave -> CH4 noise -> envelopes/length ->
 * sweep -> the quirks (zombie mode, the extra length clock).
 */
#include "gb/gb.h"

void apu_reset(gb_t *gb)
{
    memset(&gb->apu, 0, sizeof gb->apu);
}

void apu_tick(gb_t *gb, u32 tcycles)
{
    (void)gb; (void)tcycles;
    /* TODO(M10): advance the frame sequencer and every channel, then emit
     * samples into gb->apu.buffer. Called from bus_tick(). */
}

/* PROVIDED: the host drains the sample buffer once per frame. */
size_t apu_read_samples(gb_t *gb, s16 *out, size_t max)
{
    size_t n = gb->apu.buffer_len;
    if (n > max) n = max;
    if (n && out) memcpy(out, gb->apu.buffer, n * sizeof(s16));
    gb->apu.buffer_len = 0;
    return n;
}
