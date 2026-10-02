# L35 — The APU: the square channels

**Time** — theory ~25 min | coding ~55 min | verify ~10 min
**You will end with** — two square channels making musical notes, driven by the frame
sequencer. AHA: *the frequency register is a period counter, and that is the whole channel.*
**Tests that must go green** — `m10_ch1_square`, `m10_frame_sequencer_512hz`
**Depends on** — L29 (access-level ticking: the APU is clocked from `bus_tick`), L16 (a
counter and a compare). You will implement `gb/src/apu.c`.

---

## 1. Read this — the whole theory for today

### Four channels, one frame sequencer, one mixer

```
CH1 square ─┐
CH2 square ─┤
CH3 wave   ─┼─> mixer ─> apu_read_samples() ─> the host
CH4 noise  ─┘        ^
                     └── frame sequencer (512 Hz) clocks length, sweep, envelope
```

The task list lives in `apu.h`; the two functions that matter to the rest of the machine
are `apu_tick(gb, tcycles)` (called from `bus_tick`, so it receives time at 4-T granularity)
and `apu_read_samples(gb, out, max)` (the host drains it). The struct is provided with a
sample buffer; you decide how to produce samples in it.

### The register file

| Address | Register | Bits |
| --- | --- | --- |
| `FF10` | NR10 | CH1 sweep: period, direction, shift |
| `FF11` | NR11 | CH1 duty (7-6), length load (5-0) |
| `FF12` | NR12 | CH1 volume (7-4), envelope direction (3), envelope period (2-0) |
| `FF13` | NR13 | CH1 frequency, low 8 bits |
| `FF14` | NR14 | CH1 trigger (7), length enable (6), frequency high 3 bits (2-0) |
| `FF16-FF19` | NR21-NR24 | the same five, for CH2, without the sweep |
| `FF1A-FF1E` | NR30-NR34 | CH3: DAC, length, output level, frequency |
| `FF20-FF23` | NR41-NR44 | CH4: length, envelope, polynomial, trigger |
| `FF24` | NR50 | master volume: right (6-4), left (2-0) |
| `FF25` | NR51 | channel-to-side routing, one bit per channel per side |
| `FF26` | NR52 | bit 7 master power, bits 0-3 "channel is on", bits 4-6 read-only |
| `FF30-FF3F` | wave RAM | CH3's 32 samples (L36) |

Two rules about `NR52` that games depend on: writing bit 7 = 0 powers the whole APU down
and **clears every register**; writing it 1 while already on does nothing else. Reading
bits 0-3 tells a game whether a channel is currently producing sound, which is how many
games detect "the music finished".

### The frequency register is a period counter

This is the one idea that makes CH1 and CH2 trivial. The channel counts down from
`2048 - X` (where `X` is the 11-bit frequency register), and each tick of that counter
advances the duty position by one of 8 steps. So:

```
period of one full duty cycle = (2048 - X) * 4 T-cycles
frequency                     = 4194304 / ((2048 - X) * 4)
                              = 131072 / (2048 - X)
```

For `X = 1536`: `131072 / 512 = 256 Hz`. That is the number `m10_ch1_square` checks, and
it checks it by *counting level transitions*: one second at 256 Hz is 256 periods, and any
non-degenerate duty produces exactly two transitions per period, so **512 transitions** —
a value that does not depend on your mixing model, your master volume, or your sample rate.

### Duty: four 8-step patterns

`NR11` bits 7-6 pick one of:

| Duty | Pattern | High fraction |
| --- | --- | --- |
| 0 | `00000001` | 12.5% |
| 1 | `10000001` | 25% |
| 2 | `10000111` | 50% |
| 3 | `01111110` | 75% |

The duty changes *timbre*, not pitch — which is why `m10_ch1_square` re-runs the same 1536
with duty 1 and still expects 256 Hz. (The transition count is duty-independent: every
pattern above has exactly two edges per revolution.)

### The length counter, and the frame sequencer

The length counter is what stops a note. A channel with length enabled is disabled once its
counter reaches 64, and the counter is clocked at **256 Hz** — that is, on *every other*
step of the frame sequencer:

| Step (of 8) | Rate | What it clocks |
| --- | --- | --- |
| 0, 4 | 256 Hz | length |
| 2, 6 | 128 Hz | sweep (CH1 only) |
| 7 | 64 Hz | envelope |
| others | — | nothing |

The frame sequencer itself runs at **512 Hz**, so a step happens every
`4194304 / 512 = 8192` T-cycles. Length is therefore clocked every 16384 T-cycles = 256 Hz.

So a length register of 1 with length enabled lasts `64 - 1 = 63` steps:

```
63 / 256 s = 0.246 s = 1,032,192 T-cycles
```

`m10_frame_sequencer_512hz` checks the channel is still sounding at 0.20 s and silent at
0.35 s. That window is chosen to discriminate three implementations: the correct 256 Hz, a
wrong 512 Hz (silent by 0.123 s), and one that treats the register value as the step count
(silent almost immediately).

### The DAC: why `NR12 = 0` means silence

A channel's digital output is 0..15, and the "DAC" turns that into an analog level around
zero. The rule that matters:

```
DAC on  <=>  the upper five bits of NRx2 are non-zero  (NRx2 & 0xF8) != 0
```

An all-zero upper nibble means volume 0 **and** the DAC off — the channel outputs a constant
and is treated as disabled. That is why both `m10_ch1_square` and the L36 tests silence a
channel with `NR12 = 0x00` rather than with a length expiry or an `NR52` power-down, and why
`NR12 = 0xF0` (volume 15, no envelope) is the "make sound" setting they use.

### Trigger

`NRx4` bit 7 restarts the channel: reload the frequency timer, reset the duty position, reset
the envelope to the initial volume (and latch the new volume), and if the length counter is
0 reload it to 64. A game triggers a channel on every note, so a trigger that does not reset
the timer leaves you with a slightly-wrong pitch on every note after the first.

### Producing samples

You choose the sample rate (`apu.h` declares your interface; `apu_sample_rate()` reports it
for the host). The standard shape:

```
apu_tick(gb, tcycles):
    accumulate tcycles into apu.sample_accum
    while (sample_accum >= T_PER_SAMPLE):
        sample_accum -= T_PER_SAMPLE
        advance every channel by one sample's worth (or run the channel timers)
        mix NR51/NR50 into one s16 and push it, if the buffer has room

apu_read_samples: copy out up to `max`, shift the rest down, reset the count
```

The frame sequencer is just another counter inside `apu_tick`: count T-cycles, and every
8192 advance the step and clock whatever that step clocks.

> **What matters for the code you are about to write**
>
> * `f = 131072 / (2048 - X)`, i.e. a full duty cycle is `(2048 - X) * 4` T-cycles. Get that
>   factor of 4 wrong and every note is an octave off — which no unit test catches as
>   "pitch", but `m10_ch1_square`'s transition count does.
> * The frame sequencer is 512 Hz (every 8192 T-cycles) and clocks the **length** counters on
>   every other step, i.e. at 256 Hz. `m10_frame_sequencer_512hz` measures exactly this.
> * The length counter counts **up to 64**: a register value of `n` gives `64 - n` steps. A
>   register value of 1 with length enabled is 0.246 s, not 1/256 s.
> * `(NRx2 & 0xF8) == 0` means the DAC is off → constant output, channel disabled. The tests
>   rely on this to silence a channel.
> * `NR12 = 0xF0` means volume 15 with direction 0 and period 0: a constant maximum volume,
>   which is what the frequency and wave tests use as their "make sound" setting.
> * Duty changes the pulse width, not the pitch: 12.5/25/50/75% patterns all have exactly two
>   edges per revolution.
> * `apu_tick` is called from `bus_tick`, so since L29 you receive 4 T-cycles at a time.
>   Do not assume an instruction's worth of time in one call.
> * The sample buffer is finite: drop samples rather than overflowing it, and make
>   `apu_read_samples` reset the count so the host's drain is the only consumer.
> * The tests are written to be mixing-agnostic: they measure transitions (frequency) and
>   peak-to-peak amplitude (volume). Any sane mixing passes; do not chase sample values.

---

## 2. Your task — 55 min

Work in `gb/src/apu.c`.

1. **The divider and the sample accumulator** (15 min). `apu_tick` accumulating T-cycles,
   producing samples at your chosen rate, and `apu_read_samples` draining them. Verify with
   `m10_ch1_square` being *silent-but-not-implemented-failing* first.
2. **CH1: the duty generator** (20 min). The `(2048 - X)` period counter, the 8-step duty
   pattern from the table, and the DAC rule. Run `m10_ch1_square` — its first assertion
   (amplitude > 0) tells you the DAC and trigger are right, and the transition count tells
   you the frequency is right.
3. **The frame sequencer and the length counter** (15 min). 8192-T steps, length clocked on
   steps 0/4, and disable the channel at 64. Run `m10_frame_sequencer_512hz`.
4. **CH2** (5 min). The same generator with the CH2 registers and routing — that is why
   CH1 was worth getting exactly right.

---

## 3. Prove it — 10 min

```
.\gb\build.cmd -Test m10
```
```
[       OK ] m10_ch1_square
[       OK ] m10_frame_sequencer_512hz
[  FAILED  ] m10_wave_channel          <- L36
[  FAILED  ] m10_noise_lfsr            <- L36
[  FAILED  ] m10_volume_envelope       <- L36
```
Then listen, because the tests cannot tell you it sounds *right*. Write the samples out as a
WAV — the harness already has everything you need (dump the drained samples in a scratch
test) — or add `--wav FILE` to `main.c`:

```
.\gb\build\gbemu.exe --rom <a game with music>.gb --frames 600 --wav build\music.wav
```

Listen to it. The two mistakes this catches that the tests cannot: a channel that is an
octave off (the factor of 4 in the period), and a mixer that clips because two channels are
summed at full scale without attenuation.

```
.\gb\build.cmd -Test
```
Regression: L29's access-level ticking means the APU is now clocked 4 T-cycles at a time, so
`m09_access_cycle_costs` is the canary if you accidentally call `apu_tick` from somewhere
else. Nothing outside `m10` should change.

---

## 4. If it fails

| Symptom | Cause | Find it |
| --- | --- | --- |
| `m10_ch1_square` reports no amplitude | DAC off (`NR12 & 0xF8 == 0`) or the channel is not triggered | trigger bit 7 in `NRx4` |
| The transition count is about double or half | the period is `(2048 - X) * 2` or `* 8` | it is `* 4` T-cycles |
| Transitions are 0 but amplitude is fine | the duty pattern is constant (all high or all low) | use the table; duty 2 = `10000111` |
| 8x too few transitions | you advance the duty position once per sample instead of once per period tick | advance by T-cycles |
| `m10_frame_sequencer_512hz` is silent at 0.20 s | the length clock is 512 Hz, or you counted `n` instead of `64 - n` | length is clocked every other step |
| The channel never stops | you never disable it at length 64 | count up to 64, then clear the channel-on bit |
| Pitch drifts on every note | `NRx4` trigger does not reset the frequency counter | reset the timer on trigger |
| Sample buffer overruns | `apu_tick` can generate more samples than the buffer holds | drop, do not overflow |
| The whole suite slows to a crawl | you generate a sample per T-cycle | generate at the target rate |
| `m09_access_cycle_costs` fails | the APU is being ticked twice per access | `apu_tick` only from `bus_tick` |

---

## 5. Done when

- [ ] `m10_ch1_square` and `m10_frame_sequencer_512hz` are green
- [ ] CH2 works too, with the same generator
- [ ] A WAV of a real game sounds like the game, at the right pitch
- [ ] `-Test` shows no regression outside `m10`
- [ ] You can derive `131072 / (2048 - X)` from the period counter
- [ ] You can explain why `NR12 = 0x00` silences a channel without touching the length counter
- [ ] Commit message like `feat(apu): square channels, duty, length and the frame sequencer (L35)`

---

## 6. Optional, only if you have time

* Implement the CH1 sweep (`NR10`) now: it is a counter that nudges `NR13/NR14` every
  `128 Hz` step. `dmg_sound` tests it, and it is what makes the "pew" in a laser sound.
* `docs/05-audio.md` §5.4 — the mixer and the DAC, which is where the remaining "it sounds
  wrong but the tests pass" problems live.

Next: **[L36 — Wave, noise and the stretch](L36-wave-noise-and-the-stretch.md)**
