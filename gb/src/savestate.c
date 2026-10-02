/*
 * savestate.c - gb_save() and gb_load(). YOURS TO IMPLEMENT (L32).
 *
 * The contract is in include/gb/savestate.h: what a state must contain, what it must
 * NOT contain, and the two failure rules (gb_save returns 0 when the buffer is too
 * small; gb_load refuses a buffer it did not produce and leaves the machine alone).
 *
 * A shape that works well:
 *
 *   1. a tiny writer: a u8 *p cursor plus a u8 *end, with helpers that write one
 *      byte / one u16 / one u32 and set an "overflowed" flag instead of running past
 *      the end. Check the flag once at the end and return 0 if it is set.
 *   2. a header: 4-byte magic, a version, and the payload length. Reading it back
 *      gives you the "is this ours?" check for free.
 *   3. the fields, grouped by component in the same order as the gb_t definition, so
 *      that a field added later is easy to spot as missing. A quick way to keep this
 *      honest: snapshot with struct assignment into locals is NOT what you want here
 *      (that serialises padding and pointers) - write each scalar.
 *   4. gb_load reads in the same order into the same fields. Prefer "parse into a
 *      temporary, then commit" so a truncated buffer cannot half-apply.
 *
 * While you work, m11_savestate_roundtrip is your guide: it touches every component
 * before saving, scrambles all of them, and then checks each one is back. If you find
 * yourself adding a field to the machine later (the APU in L35, for instance), add it
 * here or that test will start failing in a way that looks like a save-state bug.
 */
#include "gb/gb.h"
#include "gb/savestate.h"

size_t gb_save(const gb_t *gb, u8 *buf, size_t cap)
{
    (void)gb; (void)buf; (void)cap;
    GB_UNIMPLEMENTED("gb_save() - L32: serialise the machine into buf");
    return 0;   /* not reached: GB_UNIMPLEMENTED does not return under the tests */
}

bool gb_load(gb_t *gb, const u8 *buf, size_t len)
{
    (void)gb; (void)buf; (void)len;
    GB_UNIMPLEMENTED("gb_load() - L32: restore a state produced by gb_save()");
    return false;   /* not reached */
}
