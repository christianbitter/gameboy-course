/*
 * apu.h - the four-channel sound unit. Milestone 10.
 *
 * The STRUCT is provided. The BEHAVIOUR is yours (src/apu.c).
 */
#ifndef GB_APU_H
#define GB_APU_H

#include "gb/common.h"

typedef struct gb_s gb_t;

#define GB_APU_SAMPLE_RATE 48000
#define GB_APU_BUFFER_SIZE 4096

typedef struct {
    u8  regs[0x20];     /* FF10-FF2F, as written by software            */
    u8  wave[0x10];     /* FF30-FF3F, wave RAM                          */
    u8  nr50, nr51, nr52;

    bool powered;       /* NR52 bit 7                                   */

    /* Add whatever internal state your channels need here: phase counters,
     * envelope/length/sweep timers, LFSR, frame sequencer step. */
    u32 frame_seq_counter;
    u8  frame_seq_step;

    s16 buffer[GB_APU_BUFFER_SIZE];
    size_t buffer_len;
    u32 sample_counter;
} apu_t;

void   apu_tick        (gb_t *gb, u32 tcycles);
void   apu_reset       (gb_t *gb);

/* Drain up to `max` interleaved stereo samples. Returns how many were copied. */
size_t apu_read_samples(gb_t *gb, s16 *out, size_t max);

#endif /* GB_APU_H */
