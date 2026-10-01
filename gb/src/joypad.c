/*
 * joypad.c - the FF00 key matrix. YOURS TO IMPLEMENT.
 *
 * Milestones: M06 (matrix), M05/M06 (the interrupt edge).
 *
 * The register is two 2x4 matrices selected by bits 4 and 5 of whatever
 * software last wrote to FF00:
 *
 *   bit 4 = P14, 0 selects the DIRECTION keys
 *   bit 5 = P15, 0 selects the BUTTON keys
 *
 *   read bit:   0        1        2         3
 *   P14 (dirs)  Right    Left     Up        Down
 *   P15 (btns)  A        B        Select    Start
 *
 * A pressed key reads 0. Bits 6-7 always read 1. An unselected group reads as
 * all 1s (nothing pressed).
 *
 * The interrupt (IF bit 4) fires on a HIGH-TO-LOW TRANSITION of any selected
 * bit - that is why joypad_t keeps `prev_read`. Level does not matter, only the
 * edge, so holding a key does not re-trigger.
 */
#include "gb/gb.h"

void joypad_reset(gb_t *gb)
{
    memset(&gb->joypad, 0, sizeof gb->joypad);
    /* select == 0 means both groups selected, which matches the power-on
     * value of P1 (0xCF) with nothing held. */
}

/* PROVIDED: the host calls this when a key goes down or up. */
void joypad_set(gb_t *gb, int button, bool down)
{
    if (button < 0 || button >= GB_BTN_COUNT) return;
    if (down) gb->joypad.pressed |= (u8)(1u << button);
    else      gb->joypad.pressed &= (u8)~(1u << button);
}

/*
 * Called from bus_read(FF00). Return the 8-bit register value, store the
 * written selection in joypad.select, and raise GB_INT_JOYPAD on the falling
 * edge of any selected bit.
 */
u8 joypad_read(gb_t *gb)
{
    (void)gb;
    GB_UNIMPLEMENTED("joypad_read() - M06: build the matrix value from joypad.select");
    return 0xFF;
}
