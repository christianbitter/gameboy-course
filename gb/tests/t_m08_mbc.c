/*
 * t_m08_mbc.c - milestone 8 (banking and saves). WRITE THESE TESTS YOURSELF.
 *
 * Fixture idea: build a 64 KiB or 128 KiB ROM where each 16 KiB bank starts with
 * its bank number, then assert what appears at 0x4000 after writing to the MBC
 * command windows. That one fixture covers most of MBC1/MBC5.
 */
#include "harness.h"

TEST_TODO(m08_mbc1_bank0_quirk, "write me: writing 0 to the MBC1 ROM bank register selects bank 1")
TEST_TODO(m08_mbc1_mode_ram, "write me: mode 1 moves the 2-bit register to RAM banking")
TEST_TODO(m08_mbc3_rtc, "write me: RTC register select and the latch sequence")
TEST_TODO(m08_mbc5_9bit, "write me: MBC5 takes a 9-bit bank number and bank 0 is legal")
TEST_TODO(m08_battery_save_roundtrip, "write me: RAM writes survive cart_save + reload")
