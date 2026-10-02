/*
 * t_m10_audio.c - the APU.
 *
 * PROVIDED and real: five tests, no stubs. L35 owns the first two, L36 the rest.
 *
 * How to test an APU without testing timbre: assert PROPERTIES of the produced
 * waveform through the provided seam (apu_tick + apu_read_samples). The two
 * useful measurements are
 *
 *   - the number of LEVEL TRANSITIONS, which is a pure frequency measurement and
 *     is independent of the mixing model (a DC offset, a different master volume
 *     and a different sample rate all leave it unchanged), and
 *   - the peak-to-peak amplitude, which measures volume.
 *
 * Expected values come from the hardware's documented formulas - for CH1,
 * f = 131072 / (2048 - X) - not from the implementation, so these tests cannot
 * pass by construction.
 *
 * What is deliberately NOT asserted: the exact sample values (mixing differs
 * between emulators), the internal LFSR state (it is not exposed), and the exact
 * frame-sequencer step indices. mooneye's audio tests and a pair of ears are the
 * oracle for those.
 */
#include "harness.h"

#define TC_PER_SECOND 4194304u

typedef struct {
    u32 transitions;     /* times consecutive samples differed */
    s16 min, max;
    u32 samples;
} audio_stat_t;

/* Drain the APU's sample buffer for `tcycles` of emulated time. */
static void apu_run(gb_t *gb, u32 tcycles, audio_stat_t *st)
{
    s16 buf[512];
    bool have_prev = false;
    s16 prev = 0;

    memset(st, 0, sizeof *st);
    st->min = 32767;
    st->max = -32768;

    for (u32 done = 0; done < tcycles; done += 4096) {
        apu_tick(gb, 4096);
        size_t n = apu_read_samples(gb, buf, GB_ARRAY_LEN(buf));
        for (size_t i = 0; i < n; i++) {
            s16 s = buf[i];
            if (have_prev && s != prev) st->transitions++;
            if (s < st->min) st->min = s;
            if (s > st->max) st->max = s;
            prev = s;
            have_prev = true;
            st->samples++;
        }
    }
    if (st->samples == 0) { st->min = 0; st->max = 0; }
}

static int apu_amplitude(const audio_stat_t *st) { return (int)st->max - (int)st->min; }

static void apu_power_on(gb_t *gb)
{
    bus_write(gb, 0xFF26, 0x80);      /* NR52: APU on (clears the registers) */
    bus_write(gb, 0xFF24, 0x77);      /* NR50: max volume, both sides        */
    bus_write(gb, 0xFF25, 0xFF);      /* NR51: every channel to both outputs */
}

/* CH1 = square. X is the 11-bit frequency register value; length is disabled. */
static void ch1_trigger(gb_t *gb, u8 duty, u8 nr12, u16 x)
{
    bus_write(gb, 0xFF11, (u8)((duty << 6) | 0x00));            /* NR11 */
    bus_write(gb, 0xFF12, nr12);                                 /* NR12 */
    bus_write(gb, 0xFF13, (u8)(x & 0xFF));                       /* NR13 */
    bus_write(gb, 0xFF14, (u8)(0x80 | ((x >> 8) & 0x07)));       /* NR14: trigger */
}

/* ------------------------------------------------------------------------- */
/* L35 - the square channels                                                 */
/* ------------------------------------------------------------------------- */

TEST(m10_ch1_square)
{
    /*
     * X = 1536 means f = 131072 / (2048 - 1536) = 131072 / 512 = 256 Hz.
     * One second of emulated time therefore contains 256 periods, and any
     * non-degenerate duty cycle produces exactly two level transitions per period:
     * 512 in total, whatever the mixing or the sample rate.
     */
    gb_t *gb = t_machine((const u8[]){ 0x00 }, 1);
    TEST_ASSERT(gb != NULL, "cart_load() failed");

    apu_power_on(gb);
    ch1_trigger(gb, 2, 0xF0, 1536);       /* duty 50%, volume 15, no envelope */

    audio_stat_t st;
    apu_run(gb, TC_PER_SECOND, &st);

    TEST_ASSERT(st.samples > 0,
                "the APU produced no samples at all through apu_read_samples()");
    TEST_ASSERT(apu_amplitude(&st) > 0,
                "a square channel at volume 15 must not be silent; amplitude %d",
                apu_amplitude(&st));

    const u32 expected = 2 * 256;         /* 256 Hz, two edges per period */
    const u32 slack = expected * 6 / 100; /* sampling-rate slack */
    TEST_ASSERT(st.transitions >= expected - slack && st.transitions <= expected + slack,
                "256 Hz for one second must give about %u level transitions; got %u "
                "(X=1536, amplitude %d)",
                (unsigned)expected, (unsigned)st.transitions, apu_amplitude(&st));

    /* A different duty must not change the frequency - only the pulse width. */
    apu_power_on(gb);
    ch1_trigger(gb, 1, 0xF0, 1536);       /* duty 25% */
    audio_stat_t st2;
    apu_run(gb, TC_PER_SECOND / 4, &st2);

    const u32 q_expected = 2 * 64;        /* 0.25 s of 256 Hz */
    const u32 q_slack = q_expected * 12 / 100;
    TEST_ASSERT(st2.transitions >= q_expected - q_slack && st2.transitions <= q_expected + q_slack,
                "the duty cycle changes the pulse WIDTH, not the frequency: expected "
                "about %u transitions in 0.25 s, got %u",
                (unsigned)q_expected, (unsigned)st2.transitions);

    /* And the DAC being off must silence the channel. */
    bus_write(gb, 0xFF12, 0x00);          /* NR12 = 0: volume 0 and DAC off */
    audio_stat_t st3;
    apu_run(gb, TC_PER_SECOND / 20, &st3);
    TEST_ASSERT(st3.transitions == 0,
                "with the DAC off the channel must produce a constant output; got %u "
                "transitions", (unsigned)st3.transitions);

    t_free(gb);
}

TEST(m10_frame_sequencer_512hz)
{
    /*
     * The frame sequencer runs at 512 Hz (every 8192 T-cycles) and clocks the LENGTH
     * counters on every OTHER step, i.e. at 256 Hz. CH1's length counter counts up to
     * 64, so a length register value of 1 with length enabled lasts 63 steps:
     *
     *   63 / 256 s = 0.246 s = 1,032,192 T-cycles
     *
     * The test checks the channel is still sounding 0.20 s in and silent 0.35 s in.
     * Those bounds discriminate the real 256 Hz length clock from both wrong
     * behaviours: a 512 Hz clock would fall silent before 0.20 s, and treating the
     * register value as the step count would fall silent almost immediately.
     */
    gb_t *gb = t_machine((const u8[]){ 0x00 }, 1);
    TEST_ASSERT(gb != NULL, "cart_load() failed");

    apu_power_on(gb);

    const u16 x = 1536;                                    /* 256 Hz square */
    bus_write(gb, 0xFF11, (u8)((2u << 6) | 0x01));         /* duty 50%, length = 1 */
    bus_write(gb, 0xFF12, 0xF0);                           /* constant volume 15  */
    bus_write(gb, 0xFF13, (u8)(x & 0xFF));
    bus_write(gb, 0xFF14, (u8)(0x80 | 0x40 | ((x >> 8) & 0x07)));  /* trigger + length enable */

    audio_stat_t early;
    apu_run(gb, TC_PER_SECOND * 20 / 100, &early);          /* 0.20 s */
    TEST_ASSERT(early.transitions > 0,
                "the length counter must not have expired by 0.20 s of a 0.246 s "
                "length; got %u transitions (is the length clock 512 Hz?)",
                (unsigned)early.transitions);

    audio_stat_t late;
    apu_run(gb, TC_PER_SECOND * 15 / 100, &late);           /* a further 0.15 s */
    TEST_ASSERT(late.transitions == 0,
                "after the 0.246 s length expires the channel must be silent; got %u "
                "transitions in the window that ends at 0.35 s",
                (unsigned)late.transitions);

    t_free(gb);
}

/* ------------------------------------------------------------------------- */
/* L36 - wave, noise and envelopes                                           */
/* ------------------------------------------------------------------------- */

TEST(m10_wave_channel)
{
    /*
     * CH3 plays 4-bit samples out of wave RAM (FF30-FF3F), most significant nibble
     * first. With the DAC on and the output level at 100%, a wave RAM of zeros must
     * produce a constant (silent) output, and an alternating pattern must produce a
     * waveform - which is the direct proof that the channel reads wave RAM.
     */
    gb_t *gb = t_machine((const u8[]){ 0x00 }, 1);
    TEST_ASSERT(gb != NULL, "cart_load() failed");

    apu_power_on(gb);
    const u16 x = 1536;                                    /* 128 Hz wave */
    bus_write(gb, 0xFF1A, 0x80);                           /* NR30: DAC on   */
    bus_write(gb, 0xFF1B, 0x00);                           /* NR31: length 0 */
    bus_write(gb, 0xFF1C, 0x20);                           /* NR32: 100%     */
    bus_write(gb, 0xFF1D, (u8)(x & 0xFF));
    bus_write(gb, 0xFF1E, (u8)(0x80 | ((x >> 8) & 0x07))); /* trigger, length off */

    /* All-zero wave RAM: constant output. */
    for (u16 a = 0xFF30; a <= 0xFF3F; a++) bus_write(gb, a, 0x00);
    audio_stat_t zeros;
    apu_run(gb, TC_PER_SECOND / 10, &zeros);
    TEST_ASSERT(zeros.transitions == 0,
                "an all-zero wave RAM must produce a constant output; got %u "
                "transitions", (unsigned)zeros.transitions);

    /* An alternating pattern must produce a waveform. */
    for (int i = 0; i < 16; i++) bus_write(gb, (u16)(0xFF30 + i), (u8)((i & 1) ? 0xFF : 0x00));
    bus_write(gb, 0xFF1E, (u8)(0x80 | ((x >> 8) & 0x07)));  /* re-trigger */
    audio_stat_t pattern;
    apu_run(gb, TC_PER_SECOND / 10, &pattern);
    TEST_ASSERT(pattern.transitions > 0,
                "CH3 must read wave RAM: an alternating pattern produced %u "
                "transitions", (unsigned)pattern.transitions);
    TEST_ASSERT(apu_amplitude(&pattern) > 0,
                "CH3 at 100%% output level must not be silent; amplitude %d",
                apu_amplitude(&pattern));

    /* DAC off: silent again, whatever wave RAM holds. */
    bus_write(gb, 0xFF1A, 0x00);
    audio_stat_t off;
    apu_run(gb, TC_PER_SECOND / 10, &off);
    TEST_ASSERT(off.transitions == 0,
                "with NR30 = 0 the wave channel's DAC is off and it must be silent; "
                "got %u transitions", (unsigned)off.transitions);

    t_free(gb);
}

TEST(m10_noise_lfsr)
{
    /*
     * The noise channel's LFSR is not exposed, so this test asserts what is
     * observable: it makes sound at volume 15, it makes sound in both the 15-bit and
     * the 7-bit mode, and turning the DAC off silences it. The exact sequence is
     * mooneye's job.
     */
    gb_t *gb = t_machine((const u8[]){ 0x00 }, 1);
    TEST_ASSERT(gb != NULL, "cart_load() failed");

    apu_power_on(gb);

    /* shift 7, divisor 7 -> slow enough for the sample rate to capture it */
    bus_write(gb, 0xFF20, 0x00);          /* NR41: length 0        */
    bus_write(gb, 0xFF21, 0xF0);          /* NR42: volume 15       */
    bus_write(gb, 0xFF22, 0x77);          /* NR43: 15-bit LFSR     */
    bus_write(gb, 0xFF23, 0x80);          /* NR44: trigger         */

    audio_stat_t wide;
    apu_run(gb, TC_PER_SECOND / 20, &wide);
    TEST_ASSERT(wide.transitions > 0,
                "the noise channel at volume 15 must not be silent; got %u transitions",
                (unsigned)wide.transitions);
    TEST_ASSERT(apu_amplitude(&wide) > 0, "and its amplitude must be non-zero");

    /* The 7-bit mode (NR43 bit 3) must still produce sound. */
    bus_write(gb, 0xFF22, 0x7F);
    bus_write(gb, 0xFF23, 0x80);
    audio_stat_t narrow;
    apu_run(gb, TC_PER_SECOND / 20, &narrow);
    TEST_ASSERT(narrow.transitions > 0,
                "7-bit LFSR mode must still make sound; got %u transitions",
                (unsigned)narrow.transitions);

    /* DAC off. */
    bus_write(gb, 0xFF21, 0x00);
    audio_stat_t off;
    apu_run(gb, TC_PER_SECOND / 20, &off);
    TEST_ASSERT(off.transitions == 0,
                "with NR42 = 0 the noise DAC is off and the channel must be silent; "
                "got %u transitions", (unsigned)off.transitions);

    t_free(gb);
}

TEST(m10_volume_envelope)
{
    /*
     * NR12 = 0x09 means: starting volume 0, direction UP, envelope period 1. The
     * envelope steps every period/64 s = 1/64 s = 65,536 T-cycles, so the channel is
     * silent for the first 15.6 ms and then grows one volume step at a time.
     *
     * Measuring the peak-to-peak amplitude in an early window and a later one tests
     * the direction, the period and the fact that the volume actually reaches the
     * DAC - none of which the transition count can see.
     */
    gb_t *gb = t_machine((const u8[]){ 0x00 }, 1);
    TEST_ASSERT(gb != NULL, "cart_load() failed");

    apu_power_on(gb);
    ch1_trigger(gb, 2, 0x09, 1536);       /* volume 0, +1, period 1 */

    audio_stat_t early;
    apu_run(gb, 65536 / 2, &early);       /* half a step: still volume 0 */
    TEST_ASSERT(apu_amplitude(&early) <= 1,
                "starting at volume 0 the channel must be (near) silent before the "
                "first envelope step; amplitude %d", apu_amplitude(&early));

    audio_stat_t later;
    apu_run(gb, 65536 * 6, &later);       /* six more steps -> volume 6 */
    TEST_ASSERT(later.transitions > 0,
                "the channel must be sounding after six upward envelope steps");
    TEST_ASSERT(apu_amplitude(&later) > 3,
                "an upward envelope from 0 must raise the amplitude; window 1 was %d, "
                "window 2 is %d", apu_amplitude(&early), apu_amplitude(&later));
    TEST_ASSERT(apu_amplitude(&later) > apu_amplitude(&early),
                "the envelope must make the channel LOUDER: %d then %d",
                apu_amplitude(&early), apu_amplitude(&later));

    t_free(gb);
}
