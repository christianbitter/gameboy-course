/*
 * joypad.h - the FF00 key matrix.
 *
 * The STRUCT is provided. The MATRIX LOGIC is yours (src/joypad.c).
 */
#ifndef GB_JOYPAD_H
#define GB_JOYPAD_H

#include "gb/common.h"

typedef struct gb_s gb_t;

enum {
    GB_BTN_A = 0,
    GB_BTN_B,
    GB_BTN_SELECT,
    GB_BTN_START,
    GB_BTN_RIGHT,
    GB_BTN_LEFT,
    GB_BTN_UP,
    GB_BTN_DOWN,
    GB_BTN_COUNT
};

typedef struct {
    u8 select;    /* FF00 bits 4-5 as last written: bit 4 = directions,
                     bit 5 = buttons (0 = that group is selected)  */
    u8 pressed;   /* one bit per GB_BTN_*; 1 = held down            */
    u8 prev_read; /* last value returned by joypad_read, for the
                     high-to-low edge detection that raises IF bit 4 */
} joypad_t;

void joypad_set  (gb_t *gb, int button, bool down);
void joypad_reset(gb_t *gb);

/* Called from bus_read(FF00). Combines `select` with `pressed`, and raises the
 * joypad interrupt on a high-to-low transition of any selected bit. */
u8   joypad_read (gb_t *gb);

#endif /* GB_JOYPAD_H */
