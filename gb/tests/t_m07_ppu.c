/*
 * t_m07_ppu.c - milestone 7 (rendering). WRITE THESE TESTS YOURSELF.
 *
 * They are registered as SKIPPED so the run stays readable while you work.
 * Replace each TEST_TODO block with a real TEST() that fails first and passes
 * when the feature works. The names are the contract; keep them.
 *
 * The external gate for this milestone is dmg-acid2 (see docs/06): a unit test
 * can check a decode or a priority rule, but the face is the real acceptance.
 */
#include "harness.h"

TEST_TODO(m07_bg_tile_decode, "write me: VRAM tile bytes -> 8 pixels -> palette shades")
TEST_TODO(m07_bg_scroll_wrap, "write me: SCX/SCY wrap across the 256x256 map")
TEST_TODO(m07_window_position, "write me: WX/WY placement and the window line counter")
TEST_TODO(m07_sprite_priority_x, "write me: lower X wins, ties go to lower OAM index")
TEST_TODO(m07_sprite_10_per_line, "write me: only 10 sprites per scanline are drawn")
TEST_TODO(m07_stat_modes_timing, "write me: mode 2/3/0 order and durations within a line")
TEST_TODO(m07_lyc_coincidence, "write me: STAT bit 2 and the LYC interrupt")
TEST_TODO(m07_lcd_off_blank, "write me: LCDC.7 = 0 blanks the screen and stops the modes")
