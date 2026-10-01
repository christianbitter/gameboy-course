# M10 -- Audio: the APU, out to WAV

**Goal**: implement `apu_tick`, `apu_reset` and `apu_read_samples` in `gb/apu.c` so
that `gbemu` plays the four DMG channels to a 48000 Hz stereo WAV, and make blargg's
`dmg_sound` sub-tests 01-06 report pass.

**Estimated effort**: 3-5 evenings. The mixer and one square channel are an evening; the
frame sequencer, length counters and envelopes another; wave and noise a third; the sweep
and the sub-test quirks are the rest.

## Read first

* `docs/05-audio.md` -- the whole thing, in order. Sections 5.10 and 5.11 are the API
  you are implementing; 5.4 and 5.8 are where the timing lives.
* `docs/01-orientation.md` sections 1.2 and 1.4 -- the tick loop and save states.
* `gb/include/gb/apu.h` -- the three prototypes and the `GB_APU_*` constants. Do not
  change them without asking; the harness and the WAV dumper are built on them.

## Why this milestone exists

Audio is where "the state machine is correct" has to become "the output is continuous
over time". The APU is four independent counters, a frame sequencer and a mixer, all
clocked by the same T-cycle stream the CPU and PPU already use, so it forces you to
answer a question the other subsystems let you dodge: does your notion of a cycle
advance *evenly*, or does it only look right at frame boundaries? It also has the weakest
automated oracle, so you must build your own -- dump audio, measure it, listen to it.

## Deliverable contract

* `apu_tick(gb_t*, u32 tcycles)` advances the APU by exactly `tcycles` T-cycles: the
  frame sequencer, the four channel timers, the LFSR, and the output accumulator.
* `apu_reset(gb_t*)` puts the APU in the post-boot state: the APU powered on with all
  four channels off, so `NR52` reads `0xF0` and nothing is buffered.
* `apu_read_samples(gb_t*, s16* out, size_t max)` drains up to `max` interleaved
  stereo samples and returns the count: one sample is the pair `(left, right)`, so a
  full `GB_APU_BUFFER_SIZE` buffer is 4096 samples. It must never block, never return
  more than `max`, and never lose a sample that was produced.
* `gbemu --rom ROM --wav out.wav` writes a valid RIFF/WAVE file: 2 channels, 16-bit
  little-endian, 48000 Hz, and as many samples as the emulated frame count implies.
* `gbemu --rom ROM --dump-audio out.raw` writes the same sample stream as raw `s16`.
* `gbemu --rom ROM --headless --frames N` runs the same synthesis with no window and
  no audio device, which is what the tests use.
* `gbemu_tests m10_` passes.

## Work order

1. Extend `apu_t` in `gb/include/gb/apu.h` with whatever internal state your channels
   need -- phase counters, envelope/length/sweep timers, an LFSR, the frame-sequencer
   step. The struct, the two `GB_APU_*` constants and the three prototypes are given.
2. Implement master power in `NR52` and the "powered off: all APU registers read 0"
   read path. Implement `apu_reset`.
3. Implement the mixer and the output accumulator from `docs/05-audio.md` 5.10 and
   5.11, with `NR50` and `NR51` honoured. Make `--dump-audio` produce an all-zero,
   correctly-sized stream with every channel off. That zero stream is your first test.
4. Implement CH2 only: the duty pattern and the `(2048 - X) * 4` timer. Route it to
   both sides in `NR51` and confirm a tone appears in `--dump-audio`.
5. Implement the frame sequencer counter (512 Hz, one step per 8192 T-cycles) and
   expose enough state for `m10_frame_sequencer_512hz` to observe the step sequence.
6. Implement CH1 by reusing the square core, then CH3 wave (wave RAM, `NR30`, `NR32`)
   and CH4 noise (LFSR, width mode, divisor codes).
7. Wire the frame sequencer clocks: length counters first, then envelopes.
8. Implement the length counters and the extra-length-clock quirk from 5.8.
9. Implement the sweep unit with the negate-then-calculate order from 5.6.
10. Add the WAV writer behind `--wav` and the raw writer behind `--dump-audio` (both
    are host-side file plumbing, not emulation logic; keep them out of `gb/apu.c`).
11. Run blargg's `dmg_sound` and fix sub-tests 01-06 in order. Fix nothing in 07-12 if
    it costs you forward progress; note the failure and move on.
12. Replace the five `TEST_TODO` stubs in `gb/tests/t_m10_audio.c` with real tests, then
    commit. (`gbemu_tests <filter>` matches the filter as a substring of the test name,
    so `gbemu_tests m10_` runs all five.)

## Acceptance tests

| Test | Command | Pass criterion |
| --- | --- | --- |
| `m10_ch1_square` | `gbemu_tests m10_ch1_square` | with CH1 triggered at a known frequency and duty 2, the dumped stream is non-silent, and its measured zero-crossing rate or per-channel FFT peak matches `131072 / (2048 - X)` within one part in 100 |
| `m10_frame_sequencer_512hz` | `gbemu_tests m10_frame_sequencer_512hz` | stepping the APU 8192 T-cycles 8 times produces exactly one full 8-step sequence, and the length/envelope/sweep clocks fire on the documented steps and no others |
| `m10_wave_channel` | `gbemu_tests m10_wave_channel` | a 32-nibble wave RAM pattern written before trigger appears in the same order in the output; `NR30` bit 7 = 0 silences the channel; `NR32` levels 1/2/3 scale the amplitude by 1, 1/2 and 1/4 |
| `m10_noise_lfsr` | `gbemu_tests m10_noise_lfsr` | the LFSR sequence from `0x7FFF` matches the documented feedback for both widths, and the 7-bit mode period is 127 clocks while the 15-bit mode period is 32767 |
| `m10_volume_envelope` | `gbemu_tests m10_volume_envelope` | with a decay envelope, the channel's RMS falls monotonically across envelope clocks and then holds at volume 0; with amplify it rises to 15 and holds |
| gate | `gbemu --rom roms/dmg_sound.gb --wav out.wav` | `out.wav` plays discernible, mostly correct Game Boy audio; blargg's `dmg_sound` runs report pass for sub-tests 01 through 06 |

The frequency test is the only one that needs signal processing. Measure it however
you like, but do not "test" it by asserting a buffer differs from zero: the whole point
is that a wrong divisor still produces sound.

## Common traps

* Everything is silent -> the channel was never triggered, or `NR12`/`NR22`/`NR30` bit
  7 leaves the DAC off. Detect: read back `NR52` bits 3-0; they are 0 for a dead
  channel even if you wrote a frequency.
* `NR52` reads 0xF0 forever -> you never compute the status bits, or you "helpfully"
  return the last written value for write-only registers. Detect: write `NR12 = 0x77`,
  then read `FF12`; on DMG it must read `0xFF`.
* Tone is silent after the first frame -> the host is not draining
  `apu_read_samples` every frame, so the ring buffer overflows or the producer blocks.
  Detect: log produced vs. drained counts per frame; they must differ by at most one
  sample.
* Pitch is wrong by a clean factor of 2 or 4 -> you forgot the `* 4` T-cycles per timer
  clock, or you ticked the APU once per instruction instead of per T-cycle. Detect:
  measure the frequency of a known `X` and compare with the formula.
* Notes never stop -> length counters are not clocked, or the length-enable bit is
  being read from the wrong bit position. Detect: set `NRx4` bit 6 and watch the
  counter; if it never decrements, your frame sequencer even-step wiring is wrong.
* Growth of a click or a ramp in silence -> an unsigned 0..15 DAC value is being fed
  to `s16` without centring, so "silence" is a large positive DC value. Detect: dump a
  channel with volume 0 and assert the mean is 0 before adding the high-pass.
* `dmg_sound` 02/05 fail but everything sounds fine -> the extra length clock (5.8) is
  not modelled. Detect: no listening test will ever catch this; only the ROM will.
* The WAV plays at double or half speed -> the WAV header's sample rate does not match
  what you actually produced. Detect: check the header's byte rate against
  `frames * samples_per_frame * 4`.

## Hint ladder

**H1 -- orientation.** Before touching the mixer, make `--dump-audio` emit an exact,
known number of zero samples for a fixed `--frames` value. You are validating your
tick-to-sample conversion and your buffer plumbing while the signal is trivially
correct.

**H2 -- structure.** Keep the "channel produces an integer sample" step and the "sum,
pan, scale" step in separate functions. The first is arithmetic over 0..15 and volume;
the second is pure routing. If a channel's output looks wrong, you can then dump the
per-channel pre-mix value and tell a channel bug from a mixer bug.

**H3 -- formulas and definitions.** Square: `f = 131072 / (2048 - X)`, timer reload
`(2048 - X) * 4` T-cycles. Wave: `f = 65536 / (2048 - X)`. Noise:
`f = 524288 / r / 2^(s+1)`, with `r` from the divisor-code table and the LFSR updated
per clock. Frame sequencer: 8 steps per 512 Hz, length on even steps at 256 Hz,
envelope on steps 1/3/5/7, sweep on steps 0/2/4/6. Decimation: 48000 samples for every
4194304 T-cycles. Centring: subtract 7.

## Done when

1. `gbemu --rom roms/dmg_sound.gb --wav out.wav` produces audio you can identify, with
   no missing or repeated blocks.
2. `gbemu_tests m10_` passes all five tests.
3. blargg's `dmg_sound` reports pass for sub-tests 01-06, and you have written down
   which of 07-12 still fail and why.
4. All four channels are audible in isolation with a hand-built test ROM, and `NR51`
   moves a channel from left to right when you flip its bit.
5. A 600-frame dump of a real game is free of buffer overruns and plays at the right
   speed in an external player.

## Stretch

* Model the zombie envelope and `NR52`-power interactions correctly (5.7), then re-run
  `dmg_sound` 07-12.
* Implement the DMG wave-RAM corruption window and pass `dmg_sound` 09/10.
* Add a spectrum-plot target using ffmpeg to your `Makefile` so frequency regressions
  are visible at a glance.
* Add a `--record-start/--record-stop` pair to dump only interesting frames.

## Commit

```
apu: four channels, frame sequencer, 48 kHz output

- apu_tick/apu_reset/apu_read_samples implemented
- CH1/CH2 square, CH3 wave RAM, CH4 noise LFSR
- length, envelope and sweep clocks from the 512 Hz frame sequencer
- --wav and --dump-audio writers
- tests: m10_ch1_square, m10_frame_sequencer_512hz, m10_wave_channel,
  m10_noise_lfsr, m10_volume_envelope
- dmg_sound 01-06 pass; 07-12 tracked in TASKS.md
```
