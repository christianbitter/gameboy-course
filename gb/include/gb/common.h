/*
 * common.h - shared types, constants and fatal-error plumbing.
 *
 * Provided infrastructure. You should not need to edit this file.
 */
#ifndef GB_COMMON_H
#define GB_COMMON_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef uint8_t  u8;
typedef int8_t   s8;
typedef uint16_t u16;
typedef int16_t  s16;
typedef uint32_t u32;
typedef int32_t  s32;
typedef uint64_t u64;

/* ---- CPU flags (the F register) ------------------------------------------ */
#define GB_FLAG_Z 0x80u
#define GB_FLAG_N 0x40u
#define GB_FLAG_H 0x20u
#define GB_FLAG_C 0x10u

/* ---- Machine constants -------------------------------------------------- */
#define GB_SCREEN_W            160
#define GB_SCREEN_H            144
#define GB_CYCLES_PER_SCANLINE 456
#define GB_SCANLINES           154
#define GB_TICKS_PER_FRAME     70224u      /* 154 * 456                    */
#define GB_CLOCK_HZ            4194304u    /* 2^22 Hz master clock         */
#define GB_FRAME_RATE_HZ       59.7275

#define GB_FB_SIZE             (GB_SCREEN_W * GB_SCREEN_H)

#define GB_ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))

/* Interrupt bits, shared by IF/IE and by the peripherals that raise them. */
#define GB_INT_VBLANK 0x01u
#define GB_INT_STAT   0x02u
#define GB_INT_TIMER  0x04u
#define GB_INT_SERIAL 0x08u
#define GB_INT_JOYPAD 0x10u
#define GB_INT_MASK   0x1Fu

/* Vector address for interrupt bit n: 0x40 + n*8. */
#define GB_INT_VECTOR(bit) (u16)(0x0040u + ((bit) * 8u))

/* ---- Fatal error plumbing ----------------------------------------------
 *
 * Two distinct failure modes, and the difference matters:
 *
 *   GB_UNIMPLEMENTED(...)  - "this code does not exist yet". A bug in YOUR
 *                            emulator. The default handler prints a register
 *                            dump and aborts (in tests it is caught and marks
 *                            just that one test as failed).
 *
 *   cpu_fatal(gb, ...)     - "the guest program did something impossible"
 *                            (for example executing one of the 11 illegal
 *                            opcodes). Sets gb->fatal, dumps the trace, and
 *                            stops the machine. This is a legal, testable
 *                            outcome, not a crash.
 */
typedef void (*gb_fatal_handler_t)(const char *message);
extern gb_fatal_handler_t g_gb_fatal_handler;   /* never NULL */

void gb_unimplemented_(const char *file, int line, const char *fmt, ...)
    __attribute__((format(printf, 3, 4)));

#define GB_UNIMPLEMENTED(...) \
    gb_unimplemented_(__FILE__, __LINE__, __VA_ARGS__)

/* Convenience for "come back later" markers that must not abort. */
#define GB_NOTE(msg) fprintf(stderr, "[gb] note: %s\n", (msg))

#endif /* GB_COMMON_H */
