# 05 -- The APU: four channels and a frame sequencer

The audio half of the machine. Read `01-orientation.md` first; this doc assumes the
vocabulary and the `bus_read` / `bus_write` / `bus_tick` design from section 1.2.

## 5.1 A synthesiser, not a sound card

The APU is **four tiny independent oscillators** mixed into two output pins, each with a
frequency source, a volume, a DAC and an on/off switch.

```
  CH1 square + sweep ---\
  CH2 square -----------+--> NR51 panning --> NR50 master volume --> LEFT  (s16)
  CH3 wave RAM          |                                          \-> RIGHT (s16)
  CH4 noise (LFSR) -----/            (mono per channel: one on/off bit per side)
```

Three consequences: a channel produces **one mono signal**, and stereo appears only at
`NR51`, which decides which outputs a channel is summed into; the APU is clocked by
**T-cycles** like every other component and you resample its output; and audio is the
one subsystem where "correct" is judged by a human ear.
| CH | Name | Frequency source | Special | Stop condition |
| --- | --- | --- | --- | --- |
| 1 | Square 1 | 11-bit period `X`, `f = 131072 / (2048 - X)` | **sweep** unit can retune the period | trigger NR14 b7; length |
| 2 | Square 2 | same formula | no sweep; otherwise identical | trigger NR24 b7; length |
| 3 | Wave | 11-bit period, `f = 65536 / (2048 - X)` | 32 4-bit samples from wave RAM FF30-FF3F | trigger NR34 b7; length |
| 4 | Noise | `NR43` divisor code + clock shift, `f = 524288 / r / 2^(s+1)` | 15-bit LFSR; a 7-bit "narrow" mode | trigger NR44 b7; length |

"Trigger" is the most important verb here: the trigger bit reloads the length counter,
the envelope volume and period, and the sweep shadow, sets the LFSR to `0x7FFF`, and
rewinds the wave sample index to 0. Almost every "the channel is silent" bug is a
channel that was never triggered, or was triggered while its DAC was off.

## 5.2 The register map: FF10-FF26

`R/W` means the register reads back; `W` means write-only, and a read returns `0xFF` on
DMG. Every one of these goes through your I/O switch. `NR52` is the only register that
reads back anything useful.

| Addr | Name | Bits | Meaning | Access |
| --- | --- | --- | --- | --- |
| FF10 | NR10 | `- SSS D PPP` (7-4 period, 3 dir, 2-0 shift) | sweep period 0-7 (0 = 8), dir 0 = add / 1 = subtract, shift 0-7 | R/W |
| FF11 | NR11 | `DD LL LLLL` (7-6 duty, 5-0 length) | duty select 0-3; length load = 64 - value (0 = 64) | R/W |
| FF12 | NR12 | `VVVV D PPP` (7-4 volume, 3 dir, 2-0 period) | initial volume 0-15, dir 0 = decay / 1 = amplify, period 0-7 (0 = off) | R/W |
| FF13 | NR13 | `LLLL LLLL` | frequency low 8; write-only, does not trigger | W |
| FF14 | NR14 | `T - - - L HHH` (7 trigger, 6 len-enable, 2-0 freq high) | trigger, length enable, frequency high 3 bits | R/W |
| FF16 | NR21 | `DD LL LLLL` | as NR11, no sweep | R/W |
| FF17 | NR22 | `VVVV D PPP` | as NR12 | R/W |
| FF18 | NR23 | `LLLL LLLL` | frequency low 8 | W |
| FF19 | NR24 | `T - - - L HHH` | as NR14 | R/W |
| FF1A | NR30 | `E-------` (bit 7 = DAC on) | wave DAC enable; 0 disconnects the channel | R/W |
| FF1B | NR31 | `LLLL LLLL` | length load 0-255 (length = 256 - value) | W |
| FF1C | NR32 | `-VV-----` (bits 6-5) | output level: 0 = mute, 1 = 100%, 2 = 50%, 3 = 25% | R/W |
| FF1D | NR33 | `LLLL LLLL` | frequency low 8 | W |
| FF1E | NR34 | `T - - - L HHH` | trigger, length enable, frequency high | R/W |
| FF20 | NR41 | `--LL LLLL` (5-0 length) | length load 0-63 (length = 64 - value) | W |
| FF21 | NR42 | `VVVV D PPP` | envelope, as NR12 | R/W |
| FF22 | NR43 | `SSSS W DDD` (7-4 shift, 3 width, 2-0 divisor) | LFSR clock shift; width 0 = 15-bit / 1 = 7-bit; divisor code | R/W |
| FF23 | NR44 | `T------L` (7 trigger, 6 len-enable) | trigger, length enable | R/W |
| FF24 | NR50 | `V LLL V RRR` (7 VIN-L, 6-4 vol-L, 3 VIN-R, 2-0 vol-R) | master volume 0-7 per side; VIN bits route cartridge audio | R/W |
| FF25 | NR51 | `L4 L3 L2 L1 R4 R3 R2 R1` (7-4 left, 3-0 right) | per-channel routing: 1 = that channel goes to that side | R/W |
| FF26 | NR52 | `P - - - S4 S3 S2 S1` (7 power, 3-0 status) | master power; status bits read-only, 1 = channel active | R/W |

Three details that cost days: the frequency-low registers are write-only because the
high bits live in `NRx4`; reads of write-only APU registers return `0xFF` on DMG, not the
last value written; and length registers hold inverted load values, so writing `0`
selects the *maximum* length.

## 5.3 The DAC: why "DAC off" means silence

Each channel ends in a 4-bit DAC whose digital input is the channel's current volume.

| Channel | DAC enabled when |
| --- | --- |
| CH1 / CH2 / CH4 | `NR12` / `NR22` / `NR42` bits 7-3 are non-zero (volume or envelope direction set) |
| CH3 | `NR30` bit 7 = 1 |

While the DAC is off the output sits at the analog midpoint of the 0..15 range, which
your integer mixer sees as **0 after centring**. So `NR12 = 0x00` is not "quiet": the
channel is disconnected, its envelope cannot run, and triggering it cannot make sound.

`NR52` bit 7 is the master power switch and the nastiest reset in the machine. Writing 0
clears **every** APU register and stops the frame sequencer; while off, all APU registers
read `0` (except `NR52` bit 7 itself), so implement the read path as "if powered off,
return 0x00" instead of zeroing state on every read. Powering back on leaves a blank APU:
channels stay silent until retriggered. Status bits 3-0 are **read-only** and computed
live as "DAC on and channel not finished?"; bit 6 reads 0 on DMG and bit 5 is unused.

## 5.4 The frame sequencer

Length, envelope and sweep are not continuous: a slow global counter steps them. The
**frame sequencer** runs at 512 Hz, one step per 8192 T-cycles.

```
 step:      0     1     2     3     4     5     6     7   (then 0 again)
 length:    x           x           x           x            256 Hz  (even steps)
 envelope:        x           x           x           x        64 Hz  (odd steps)
 sweep:     x           x           x           x              128 Hz  (even steps)
```

Clock it from your T-cycle count (`counter++` every 8192 T-cycles). Which channel a
length or envelope clock applies to depends on that channel's length-enable bit. The
sequencer runs forever while the APU is powered; `NR52` bit 7 = 0 resets its step.

## 5.5 Square channels: frequency and duty

The 11-bit period `X = (NRx4 & 7) << 8 | NRx3` is a *divider*, not a frequency:

```
f_out = 131072 / (2048 - X) Hz        X = 0 -> silence (divider 2048 never expires)
```

The waveform is an 8-step duty pattern (one period = 8 timer expiries), selected by
`NRx1` bits 7-6. Count the ones and derive the percentages yourself.

| Duty | Pattern (8 steps) | Approx. duty |
| --- | --- | --- |
| 0 | `0000 0001` | 12.5% |
| 1 | `1000 0001` | 25% |
| 2 | `1000 0111` | 50% |
| 3 | `0111 1110` | 75% |

Shape of the algorithm, for you to complete:

```
step_square(ch):
    ch.timer -= 1
    if ch.timer == 0:
        ch.timer = (2048 - X) * 4            # TODO: derive the *4
        ch.duty_pos = (ch.duty_pos + 1) & 7
    output = (duty_pattern[ch.duty] >> ch.duty_pos) & 1    # scaled by volume
```

**Question before coding:** each duty step lasts `(2048 - X)` timer clocks, and each
timer clock is 4 T-cycles. Confirm that this factor of 4 is exactly what turns the code
above into `131072 / (2048 - X)`, then find the same factor in the noise formula.

## 5.6 Sweep (CH1 only)

`NR10` gives a period (0-7, 0 = 8), a direction (0 = add, 1 = subtract) and a shift
(0-7). On each sweep clock (steps 0, 2, 4, 6):

```
sweep_tick():
    period_counter -= 1
    if period_counter == 0:
        period_counter = nr10_period (0 -> 8)
        if nr10_period > 0 or shift > 0:        # sweep enabled at all
            shadow = calculate()
            nrx3, nrx4 = split(shadow)          # TODO: the split and the 0x7FF mask
```

`calculate()` is where the famous quirk lives, and it is a **sequence**, not a formula:
copy the shadow period; if the direction is **subtract**, apply the subtraction
**before** the overflow check, and a negative result disables the channel immediately;
shift the (possibly negated) value right by `shift`; apply add or subtract to the
shadow; and if the result exceeds 2047, disable the channel by clearing `NRx4` bit 7
and the `NR52` status bit for CH1. Implementations that test overflow *before* negating
get the wrong answer on exactly the ROMs that test it.

On trigger, an overflow check runs `calculate()` and disables the channel if it
overflows, but that disabled state lasts only until the next sweep clock, which
re-enables it for one more iteration. Implement it literally once the basics work.
## 5.7 Envelope

`NRx2` bits 7-4 = current volume, bit 3 = direction (0 = decay, 1 = amplify),
bits 2-0 = period (0 = envelope off). On each envelope clock (steps 1, 3, 5, 7):

```
envelope_tick():
    period_counter -= 1
    if period_counter == 0:
        period_counter = nrx2_period (0 -> 8)
        if period == 0: return
        volume += (direction ? +1 : -1)      # TODO: clamp 0..15, stop when saturated
```

The **zombie mode** quirk: while a channel is active, writing `NRx2` makes the volume
register transiently read the last *written* value instead of the live one, and the
next envelope tick can resurrect a stale volume. blargg tests it. Know it exists; do
not require it in your first pass. Triggering with period 0 and a non-zero volume is
the normal way to get a constant tone, because then the envelope never ticks.

## 5.8 Length counters

Load values are inverted, so 0 means maximum:

```
CH1/CH2/CH4: length = 64 - (NRx1/NR21/NR41 & 0x3F)        CH3: length = 256 - NR31
```

The "64-step table" is just "1 step per length clock at 256 Hz": 64 steps = 0.25 s for
the three 6-bit channels, and 256 steps = 1.0 s for CH3. The counter decrements only if
**length enable** (`NRx4` bit 6) is set, on the length clock (even sequencer steps).
Reaching zero disables the channel: clear `NRx4` bit 7 and the `NR52` status bit.

The **extra length clock** quirk: enabling length (`NRx4` bit 6 = 1) on a channel that
is already live clocks the counter once immediately when (a) the *next* sequencer step
is odd (1, 3, 5, 7) and (b) the counter is non-zero. If the counter was 1, the channel
dies at once. In hardware this is "the enable logic can see the current step", so model
it by remembering the step and deciding the extra clock on the enable write.

## 5.9 Noise: the LFSR

CH4 has no oscillator, only a shift register clocked by a divider.

```
NR43: bits 7-4 = s (clock shift)   bit 3 = w (width)   bits 2-0 = divisor code
f_clock = 524288 / r / 2^(s+1) Hz
divisor codes: 0 -> 0.5   1 -> 1   2 -> 2   3 -> 3   4 -> 4   5 -> 5   6 -> 6   7 -> 7
```

Write the update yourself, then verify it with your own LFSR unit test:

```
lfsr_update():
    bit = (lfsr ^ (lfsr >> 1)) & 1           # XOR of bits 0 and 1
    lfsr >>= 1
    lfsr |= bit << 14
    if width_mode == 1:                       # 7-bit mode also taps bit 6
        lfsr = (lfsr & ~(1 << 6)) | (bit << 6)     # TODO: check the mask and OR
```

Reset value on trigger and on power-up is `0x7FFF`; correct feedback avoids the zero
state entirely, and a frozen LFSR outputs 1. 7-bit mode is what turns hiss into a tone,
because the maximum sequence drops from 32767 to 127 and the noise becomes audibly
periodic. Divisor code 0 (`r = 0.5`) has a hardware off-by-one that blargg tests:
compare `r = 8`-style integers when a sub-test complains, but do not chase it first.

## 5.10 Getting samples out: tick, accumulate, decimate

The host drains a stereo buffer once per frame through the API contract that
`gb/include/gb/apu.h` already declares:

```c
/* apu.h -- the infrastructure defines the struct and these three prototypes */
#define GB_APU_SAMPLE_RATE 48000
#define GB_APU_BUFFER_SIZE 4096

typedef struct {
    u8  regs[0x20];   /* FF10-FF2F, as written by software */
    u8  wave[0x10];   /* FF30-FF3F, wave RAM                */
    u8  nr50, nr51, nr52;
    bool powered;     /* NR52 bit 7                         */
    /* ... your channel state, frame sequencer, LFSR, ... */
    s16 buffer[GB_APU_BUFFER_SIZE];   /* interleaved (L,R) pairs */
    size_t buffer_len;
} apu_t;

void   apu_tick(gb_t *gb, u32 tcycles);
void   apu_reset(gb_t *gb);
size_t apu_read_samples(gb_t *gb, s16 *out, size_t max);
```

The APU runs on T-cycles and the host wants 48000 Hz. You may not pretend one output
sample is one T-cycle, and you do not need resampling filters:

```
T-cycles per output sample = 4194304 / 48000 = 87.38...   (~87.4 T-cycles)
T-cycles per video frame   = 70224
output samples per frame   = 70224 * 48000 / 4194304 = 803.6...   (so 803 or 804)
```

Use an integer accumulator so the fraction never drifts:

```
tick(tcycles):
    for each of tcycles:
        advance frame sequencer, square/wave/noise timers, LFSR
        accum += 48000
        if accum >= 4194304:
            accum -= 4194304
            push mixed stereo sample into ring buffer      # TODO: the exact rational
read_samples(out, max): drain min(available, max) samples from the ring buffer
```

The TODO is the decimation rational: reduce `48000` and `4194304` by their common
factor (or keep the threshold an exact multiple) so long runs never drift or duplicate
samples. Why the naive version still sounds recognisable: one sample per ~87 T-cycles
*is* a 48 kHz sample, the mixer is a pure integer sum, and nothing in the chain is
band-limited anyway, so the error is mild aliasing, never wrong pitch. Optional
refinement once the sound is right: a DC-blocking high-pass
(`y[n] = x[n] - x[n-1] + 0.999 * y[n-1]`) removes the offset from centring and the click
when a loud constant channel starts.
## 5.11 Mixing

| Channel | DAC output | Centred | Notes |
| --- | --- | --- | --- |
| CH1, CH2, CH4 | 0..15 from `NRx2` volume | `value - 7` -> `-7..8` | asymmetric: the true midpoint is 7.5 |
| CH3 | 0..15 from wave RAM scaled by `NR32` | `value - 7` | `NR32` scales the sample, not the DAC |

```
left  = sum(centred output of each CH with its NR51 left  bit set) * NR50 left  volume
right = sum(centred output of each CH with its NR51 right bit set) * NR50 right volume
```

`NR50` volumes are 0-7: 0 is silence, 7 is maximum, and games use 7/7 or 7/6. Bits 5
and 1 are the VIN bits that select cartridge audio; on DMG there is none, so model them
as routing nothing. `NR52` bit 7 = 0 silences everything, and a channel whose DAC is
off contributes 0 (already centred), not `-7`. Nothing filters the sum: sum in 32-bit
and let the host clamp, or scale down, because a four-channel sum at volume 7 overflows
`s16` on its own.

## 5.12 Why audio is the least unit-testable subsystem

Every other subsystem has a binary oracle: the frame bytes match or they do not, but
audio has a *timbre*, and "sounds vaguely like Tetris" is not an assertion.

| Layer | What you can test | Tool |
| --- | --- | --- |
| Register semantics | read-back, power-off reset, status bits | your C unit tests |
| Counters | sequencer at 512 Hz, length at 256 Hz, envelope steps | your C unit tests |
| Frequency | zero-crossing or FFT peak vs `131072/(2048-X)` | `--dump-audio`, FFT |
| Timbre | nothing automated | ears |

`dmg_sound` is your oracle for the first two layers. Sub-tests by subject and by how
timing-sensitive they are:

| Sub-tests | Subject | Timing |
| --- | --- | --- |
| 01, 03, 08 | register read/write, trigger, `NR52` power-off reset | low |
| 06, 07 | wave RAM / `NR30` / `NR32`, noise LFSR and 7-bit mode | low |
| 02, 04, 05, 11 | length (extra length clock), sweep negate, sequencer timing | high |
| 09, 10, 12 | wave RAM read + DMG fetch corruption, full-precision frequency | cycle-exact |

Passing 01-06 is the milestone gate. 09-12 are the deep end: 10 measures the CH3 fetch
window and 12 is a stopwatch on your T-cycle accounting, so both fail until the whole
machine's timing is exact. Harness: `gbemu --rom roms/tetris.gb --frames 1800 --wav
out.wav` and listen; `--headless` on `roms/dmg_sound.gb` for serial pass/fail text;
`ffmpeg -i out.wav -lavfi showspectrumpic spec.png out.wav` for the spectrum.

## 5.13 What to implement first

Do not implement the whole of 5.2 at once. Build a vertical slice you can hear.

1. **State + power**: an `apu_t` with four channel structs, `NR52` bit 7 semantics
   (off = registers read 0), and `apu_reset`.
2. **Mixing + output**: `NR50`, `NR51`, the mixer, the accumulator/downsampler, and
   `apu_read_samples`. Verify silence: all channels off must give an all-zero
   `--dump-audio` and a valid silent WAV of the right duration.
3. **CH2 square only** (simplest: no sweep, no wave RAM): duty, frequency, `NRx2`
   volume. Route CH2 to both sides in `NR51` and you hear a tone.
4. **Frame sequencer**: the 512 Hz counter and step table from 5.4, wired to nothing.
5. **Length counters**, then **envelopes** -- the reasons notes stop and fade.
6. **CH1 with sweep**, reusing the square core plus the shadow/negate logic.
7. **CH4 noise**: LFSR, then width mode, then divisor codes.
8. **CH3 wave**: wave RAM, `NR30`, `NR32`; a *sample player* with its own timer at
   `65536/(2048-X)`.
9. **Quirks**: extra length clock, sweep negate-overflow, zombie envelope, wave RAM
   corruption, noise divisor 0 -- in that order of pain.

## Sources

* **Pan Docs:** *Audio* (overview and the `NR52`/`NR51` traps), *Audio Registers*
  (`NR10`-`NR52` and wave RAM, register by register, with the DMG read/write masks),
  *Power Up Sequence* (the post-boot values of `FF10`-`FF26`, which `dmg_sound`
  sub-test 01 reads back — the table is in
  [../reference/cheatsheet-flags-and-timing.md](../reference/cheatsheet-flags-and-timing.md)).
* **Oracles:** blargg's `dmg_sound` (12 sub-tests; 01-06 is the milestone gate, and
  this document says which of the rest are timing-sensitive and why), plus
  `--dump-audio` / `--wav` and an FFT for the frequency checks.
* Annotated links: [../reference/external-references.md](../reference/external-references.md).
* Honest gap: the exact wording of the sweep-negate and noise-divisor-0 quirks was
  not verified against a fetched source here. Treat §5.9's quirk list as a starting
  point and let `dmg_sound` 05/07 arbitrate.

Next: **[06-verification-and-tooling.md](06-verification-and-tooling.md)** -- how you will
know any of this is right, and the timing work the last `dmg_sound` sub-tests are really
measuring.
