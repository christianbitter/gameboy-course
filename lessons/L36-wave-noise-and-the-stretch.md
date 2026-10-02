# L36 — Wave, noise and the stretch

**Time** — theory ~30 min | coding ~45 min | verify ~15 min
**You will end with** — the last two channels, and a decision: stop here (the DMG emulator is
finished) or take the optional CGB stretch. AHA: *the wave channel reads RAM; the noise
channel is a shift register; an envelope is a counter.*
**Tests that must go green** — `m10_wave_channel`, `m10_noise_lfsr`, `m10_volume_envelope`
**Optional stretch** — `m12_cgb_palettes`, `m12_double_speed`
**Depends on** — L35 (the frame sequencer, the DAC rule, the sample path). You will implement
the rest of `gb/src/apu.c`.

---

## 1. Read this — the whole theory for today

### CH3, the wave channel: a RAM readout

CH3 plays 4-bit samples straight out of wave RAM (`FF30-FF3F`, 16 bytes = **32 samples**,
high nibble first). No duty pattern, no volume counter — just a clock and a pointer into
memory.

| Register | Meaning |
| --- | --- |
| `NR30` (`FF1A`) | bit 7 = DAC on |
| `NR31` (`FF1B`) | length load |
| `NR32` (`FF1C`) | bits 6-5 = output level: 0 = mute, 1 = 100%, 2 = 50%, 3 = 25% |
| `NR33`/`NR34` | frequency (11 bits) and trigger, exactly as for CH1 |

The frequency formula differs from the square channels by a factor of two, and the reason is
worth seeing:

```
one wave cycle = 32 samples
each sample lasts (2048 - X) * 2 T-cycles     (two T per sample step)
=> f = 4194304 / ((2048 - X) * 2 * 32) = 65536 / (2048 - X)
```

So for `X = 1536`: `65536 / 512 = 128 Hz`, half what the same `X` gives a square channel.
Getting that factor of 2 wrong is the classic CH3 bug.

`m10_wave_channel` proves the channel reads RAM, which is the whole point of CH3: with the
DAC on and the level at 100%, an all-zero wave RAM must produce a **constant** output, an
alternating pattern must produce a **waveform**, and turning the DAC off must silence both.

### CH4, the noise channel: an LFSR

| Register | Meaning |
| --- | --- |
| `NR41` (`FF20`) | length load |
| `NR42` (`FF21`) | volume, envelope, and the DAC-on rule |
| `NR43` (`FF22`) | bits 7-4 divisor `r`, bit 3 width (0 = 15-bit, 1 = 7-bit), bits 2-0 shift `s` |
| `NR44` (`FF23`) | trigger, length enable |

The noise source is a linear-feedback shift register. Each clock:

```
bit  = (lfsr ^ (lfsr >> 1)) & 1
lfsr = (lfsr >> 1) | (bit << 14)
if (width_7bit) lfsr = (lfsr & ~0x40) | (bit << 6)     /* also feed bit 6 */
```

and the channel's output is the inverse of the LFSR's low bit (`~(lfsr & 1) & 1`). The clock is

```
f = 524288 / r / 2^(s + 1)          /* r = 0 means 0.5, the fastest setting */
```

A 15-bit LFSR has a period of 32767 clocks, which is the "chhhhh" you hear; the 7-bit mode
wraps after 127 clocks and sounds like a harsh buzz. `m10_noise_lfsr` asserts what is
observable — sound at volume 15 in both widths, silence with the DAC off — because the LFSR's
internal state is not part of the public interface. If you want the sequence checked
precisely, mooneye's audio tests are the oracle.

### The envelope: a volume counter

`NRx2` bits 7-4 hold the current volume, and the envelope changes it:

| Bits | Meaning |
| --- | --- |
| 3 | direction: 0 = attenuate (decrease), 1 = amplify (increase) |
| 2-0 | period `n`: a step every `n / 64` seconds, and **0 means the envelope is off** |

The envelope is clocked by frame-sequencer step 7 (64 Hz), and a step is skipped `n - 1`
times out of `n`. The volume clamps at 0 and 15.

`m10_volume_envelope` starts a channel at volume 0 with an upward envelope of period 1 and
measures the peak-to-peak amplitude in two windows: the first 0.01 s (before the first step,
so silent) and a later window after ~6 steps (audible and louder). That tests the direction,
the period and the fact that the volume actually reaches the DAC — none of which the
transition count can see.

### The mixer, and the last thing to get right

```
level = 0
for each channel enabled by NR52 and routed by NR51:
    level += channel_digital_output      /* -15..15 once centred */
level = level * master_volume / 7         /* NR50, per side */
```

Two practical notes: keep the digital outputs centred around zero (that is what makes a
silent channel silent instead of a DC offset), and attenuate before you clamp so that four
channels at volume 15 do not clip into a square wave.

### Then the decision: the stretch

Stop here and you have a complete DMG emulator. The two `m12_*` tests stay red, deliberately:
they are an optional goal, not a debt. If you take the stretch, the surface is already
declared for you:

| Piece | Where it lives | What to implement |
| --- | --- | --- |
| `gb->cgb` | `include/gb/gb.h` | the machine's mode, set at load time from header `0x0143` bit 7 (`0x80` = CGB-enhanced, `0xC0` = CGB-only, `0x00` = DMG) — record it in `cart.cgb_capable` |
| `ppu.cgb_bg_ram[64]`, `ppu.cgb_obj_ram[64]`, `ppu.bcps`, `ppu.ocps` | `include/gb/ppu.h` | palette RAM and its index registers: `FF68`/`FF69` (BG) and `FF6A`/`FF6B` (OBJ), 6-bit index, bit 7 = auto-increment, writes advance the index |
| `gb->key1` | `include/gb/gb.h` | `FF4D`: bit 0 = arm, bit 7 = current speed; the next `STOP` performs the switch |

**The double-speed ratio is the interesting part.** When the switch is armed and `STOP`
executes, the CPU's clock doubles while the PPU and APU keep their own rate. In practice that
means the PPU must advance by **half** as many dots per CPU T-cycle as before — which is what
`m12_double_speed` measures with 200 NOPs at each speed. A save state saved before the switch
and loaded after must restore the speed too.

**And this is where L32's warning comes due.** You added machine state: `cgb`, `key1`, the
palette RAM and the index registers. Add them to `savestate.c` — the file said so, and now it
is literally true. (The round-trip test still passes if you forget, which is exactly the trap
L32 described.)

> **What matters for the code you are about to write**
>
> * CH3's frequency is `65536 / (2048 - X)` — half the square channels' — because one wave
>   cycle is 32 samples and a sample step is 2 T-cycles. Getting this wrong makes CH3 an
>   octave off.
> * CH3's wave RAM is 16 bytes and the **high nibble comes first**. Sample `k` is
>   `(wave[k/2] >> (k % 2 ? 0 : 4)) & 0x0F`.
> * `NR32` bits 6-5 are an output *level*: 0 = mute, 1 = 100%, 2 = 50%, 3 = 25%. Note that
>   0 mutes the channel even with the DAC on, which is a different silence from `NR30 = 0`.
> * CH4's LFSR is 15 bits with taps 0 and 1; the 7-bit mode also feeds bit 6. Output is the
>   **inverse** of bit 0.
> * CH4's clock is `524288 / r / 2^(s+1)` with `r = 0` meaning 0.5. `m10_noise_lfsr` uses
>   divisor 7, shift 7 for a rate the sample path can actually reproduce.
> * The envelope steps on frame-sequencer step 7 with the `n - 1` skip rule, clamps at 0 and
>   15, and is **off** when `n = 0`. `m10_volume_envelope` uses direction up, period 1.
> * All three L36 channels obey the same DAC rule as L35: `(NRx2 & 0xF8) == 0` means the DAC
>   is off and the channel is silent. The tests silence channels that way.
> * The stretch is optional. If you skip it, the course is complete at
>   **75 of 77 tests green** and the two `m12_*` failures are your own choice, not unfinished
>   work. If you take it, `gb->cgb` must stay **false** for a DMG cartridge: on a DMG,
>   `FF68-FF6B` and `FF4D` do not exist, and both stretch tests check that.
> * The CGB work is additive: the palette RAM and `key1` are new fields, so anything that
>   already passed must keep passing, and `savestate.c` grows by five fields.

---

## 2. Your task — 45 min

Work in `gb/src/apu.c`.

1. **CH3** (15 min). Wave RAM readout, the 32-sample pointer, `NR32`'s output level, and the
   `65536 / (2048 - X)` clock. Run `m10_wave_channel`.
2. **CH4** (15 min). The LFSR with both widths, the divisor/shift clock, the length counter
   from L35. Run `m10_noise_lfsr`.
3. **The envelope** (10 min). The step-7 clock, the `n - 1` skip, direction, clamping. Run
   `m10_volume_envelope`.
4. **The full suite** (5 min):
   ```
   .\gb\build.cmd -Test
   ```
   `m10` green, and everything else unchanged.

---

## 3. Prove it — 15 min

```
.\gb\build.cmd -Test
```
```
   75 passed,    2 failed,    0 skipped
```
The two failures are `m12_cgb_palettes` and `m12_double_speed` — the stretch you have not
chosen yet. **This is the DMG emulator, finished.** Do the three things that make that claim
honest:

```
:: 1. the whole external gate, in one sitting
.\gb\build\gbemu.exe --rom roms\blargg\cpu_instrs\cpu_instrs.gb --serial - --frames 30000
.\gb\build\gbemu.exe --rom roms\blargg\instr_timing.gb          --serial - --frames 2000
.\gb\build\gbemu.exe --rom roms\blargg\mem_timing.gb            --serial - --frames 2000
.\gb\build\gbemu.exe --rom roms\blargg\halt_bug.gb              --serial - --frames 4000
.\gb\build\gbemu.exe --rom roms\dmg-acid2.gb --frames 60 --dump-frame build\acid2.bmp
python tools\compare_bmp.py build\acid2.bmp build\reference.bmp

:: 2. a game, with sound, in the window from L34
.\gb\build\gbemu.exe --rom <a real game>.gb

:: 3. the written record
```

Step 3 is `NOTES.md`: the `TODO(hardware)` list from L31, the golden `dmg-acid2` frame, and
the pass/fail table from L31 §3 updated to today. That file is the honest state of your
emulator, and it is the thing you would hand someone if you asked for help.

### The stretch, if you are taking it

Implement the palette RAM and `KEY1` behind `gb->cgb`, and run:

```
.\gb\build.cmd -Test m12
```
```
   2 passed,    0 failed,    0 skipped
```
Then remember `savestate.c`: add `cgb`, `key1`, `cgb_bg_ram`, `cgb_obj_ram`, `bcps` and
`ocps`, and re-run `m11_savestate_roundtrip` (which will still pass if you forgot — so check
the fields by eye, then save and load across a speed switch and confirm the speed survives).

---

## 4. If it fails

| Symptom | Cause | Find it |
| --- | --- | --- |
| `m10_wave_channel` sees sound from zeroed wave RAM | you are not reading the RAM (or the high nibble is wrong) | sample `k` = `(wave[k/2] >> (k%2 ? 0 : 4)) & 0xF` |
| CH3 is an octave off | you used the square formula | it is `65536 / (2048 - X)` |
| CH3 is silent with `NR32 = 0x00` but not with `NR30 = 0` | `NR32` bits 6-5 = 0 mutes the channel | both are silences, and they differ |
| `m10_noise_lfsr` is silent | the DAC rule, or the LFSR never leaves its seed | seed is non-zero; check `NR42 & 0xF8` |
| Noise is a single tone | you clock the LFSR at a fixed rate | `524288 / r / 2^(s+1)` |
| 7-bit mode sounds the same as 15-bit | you never feed bit 6 | the width bit changes the taps |
| `m10_volume_envelope` amplitude never grows | direction bit reversed, or the channel was not triggered at volume 0 | bit 3 set = amplify |
| The envelope steps every frame | you clocked it on every sequencer step | step 7 only, with the `n - 1` skip |
| The stretch test fails with `gb->cgb = true` | the registers are unimplemented in `bus.c` | `FF4D`, `FF68-FF6B` |
| `m12_double_speed`'s ratio is ~1.0 | the PPU is ticked with CPU cycles, not half of them | the PPU keeps its own rate |
| The ratio is ~2.0 | you halved the CPU instead of the PPU | double speed doubles the CPU |
| Sound has a DC offset when silent | an uncentred channel output | centre the digital value around zero |

---

## 5. Done when

- [ ] `m10_wave_channel`, `m10_noise_lfsr` and `m10_volume_envelope` are green
- [ ] `-Test` reports 75 passed, 2 failed (the stretch), 0 skipped
- [ ] The six external gates from L31's table pass, with `dmg-acid2` at 0 differing pixels
- [ ] A real game plays in the window with working sound
- [ ] `NOTES.md` holds the `TODO(hardware)` list, the golden frame, and the gate table
- [ ] You can state, for each of the four channels, what produces its waveform
- [ ] **Optional:** `m12_cgb_palettes` and `m12_double_speed` are green, and `savestate.c` knows about the new fields
- [ ] Commit message like `feat(apu): wave, noise and envelopes - the DMG emulator is complete (L36)`

---

## 6. Optional, only if you have time

* `docs/05-audio.md` §5.5 — the `dmg_sound` test suite, which is where you go if you want the
  length/sweep/DAC corner cases verified by something other than your own ears.
* Write your own test ROM with RGBDS (`PREREQUISITES.md` lists the package) and drive it with
  the harness. You have a disassembler now (L33), so reading your own ROM's trace is easy —
  and that is the point at which this stops being a course and becomes your toolchain.

**The course is finished here.** The ladder in [LESSONS.md](../LESSONS.md) is the map of what
you built, and `milestones/` holds the longer-form briefs you may want to revisit.
