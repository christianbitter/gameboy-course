# L17 — The last opcodes, and serial

**Time** — theory ~20 min | coding ~60 min | verify ~10 min
**You will end with** — **every base opcode except `STOP`**, and a serial port you can
print through. That combination is what makes the next lesson's `cpu_instrs` run
possible.
**Tests that must go green** — `m05_serial_emits_byte` (and with it, all eight `m05_*`
tests)
**Depends on** — L03 (the I/O ownership table), L15 (`bus_request_interrupt`). You will
edit `gb/src/opcodes.c`, `gb/src/serial.c` and `gb/src/bus.c`.

---

## 1. Read this — the whole theory for today

### The opcodes you have left

Everything else is done. These are the remainder, and they are all short:

| Opcode | Instruction | Bytes | T-cycles | The tricky part |
| --- | --- | --- | --- | --- |
| `0xE0` | `LDH (a8),A` | 2 | 12 | writes `A` to `0xFF00 + a8` |
| `0xF0` | `LDH A,(a8)` | 2 | 12 | reads from `0xFF00 + a8` |
| `0xE2` | `LD (C),A` | 1 | 8 | address is `0xFF00 + C` |
| `0xF2` | `LD A,(C)` | 1 | 8 | address is `0xFF00 + C` |
| `0xE8` | `ADD SP,e8` | 2 | 16 | signed offset; flags from the low byte only |
| `0xF8` | `LD HL,SP+e8` | 2 | 12 | same flag rule, but writes `HL` |
| `0xE9` | `JP HL` | 1 | 4 | `PC = HL`; no flags, no memory |
| `0xF9` | `LD SP,HL` | 1 | 8 | |
| `0x08` | `LD (a16),SP` | 3 | 20 | low byte to `a16`, high byte to `a16+1` |
| `0x09` `0x19` `0x29` `0x39` | `ADD HL,rr` | 1 | 8 | `Z` **unchanged**, `H` from bit 11 |
| `0x03` `0x13` `0x23` `0x33` | `INC rr` | 1 | 8 | **no flags at all** |
| `0x0B` `0x1B` `0x2B` `0x3B` | `DEC rr` | 1 | 8 | **no flags at all** |

If you took the optional steps in L07 you already have `0x02/0x12/0x0A/0x1A/0x22/0x2A/
0x32/0x3A`; if you took L13's option you have the 16-bit pairs. Check with
`.\gb\build.cmd -Test` and your `NOTES.md` debt list, and implement whatever is missing.

`LDH` exists because the 8080 heritage could only address 256 bytes of I/O directly.
Games use it constantly to talk to the PPU, timer and joypad registers:

```
LDH (a8),A   ->   bus_write(gb, 0xFF00 | cpu_fetch8(gb), gb->cpu.a);
LDH A,(a8)   ->   gb->cpu.a = bus_read(gb, 0xFF00 | cpu_fetch8(gb));
```

`LD (C),A` and `LD A,(C)` are the same thing with `C` supplying the low byte instead of
an immediate — note that `0xFF00 + C` never touches `B`, and the two forms are the same
instruction shape.

### `ADD SP,e8` and `LD HL,SP+e8` have their own flag rule

Both add a **signed 8-bit** offset to `SP`, and both compute flags from the **low byte
only**, using the offset interpreted as *unsigned* for the flag test:

```
offset = (int8_t)cpu_fetch8(gb)
result = SP + offset                      /* 16-bit wrap; for LD HL,SP+e8 -> HL */
Z = 0
N = 0
H = ((SP & 0x0F) + (offset & 0x0F)) > 0x0F
C = ((SP & 0xFF) + (offset & 0xFF)) > 0xFF
```

Worked example, `SP = 0xFFF8`:

| instruction | offset | result | H | C | why |
| --- | --- | --- | --- | --- | --- |
| `ADD SP,e8` | `0x08` | `0x0000` | 1 | 1 | `8+8 = 16` -> H; `0xF8+0x08 = 0x100` -> C |
| `ADD SP,e8` | `0xF8` (-8) | `0xFFF0` | 1 | 1 | low nibbles `8+8` -> H; `0xF8+0xF8 = 0x1F0` -> C |

Note the second row: the offset is *negative* for the result but its low nibble is still
`8`, and it still produces `H` and `C`. This is why the rule says "unsigned for the
flags, signed for the arithmetic".

### The serial port is 8 bytes of register and one shortcut

| Register | Address | Meaning |
| --- | --- | --- |
| `SB` | `FF01` | the byte being shifted |
| `SC` | `FF02` | bit 7 = transfer in progress / done, bit 1 = internal clock, bit 0 = shift clock |

On real hardware, `SC = 0x81` starts a transfer at 8192 Hz: one bit every 512 T-cycles,
4096 T-cycles for a byte, and the other end arrives through the link cable. You have no
link partner, so there is nothing to receive — but **every test ROM uses this port to
print its verdict**, and all of them poll `SC` bit 7 to find out when to send the next
character. That gives a legitimate shortcut:

```
on writing SC = 0x81:
    gb_serial_byte(gb, gb->serial.sb);        /* provided: prints the character */
    gb->serial.sc = 0x01;                     /* bit 7 clear = "transfer finished" */
    bus_request_interrupt(gb, GB_INT_SERIAL); /* IF bit 3 */
```

`gb_serial_byte()` is provided in `gb/src/gb.c` and writes to `gb->serial_out`, which
`main.c` points at a file or at stdout when you pass `--serial -`. So once this is
implemented, `--serial -` becomes your `printf` for the rest of the course, and the
next lesson's gate becomes readable.

Modelling the real 4096-cycle transfer instead of the shortcut is L30's business. The
shortcut passes every test ROM you will use, and the lesson tells you why rather than
pretending it is exact.

> **What matters for the code you are about to write**
>
> * `LDH`/`LD (C)` addresses are `0xFF00 | low_byte` — a write to `FF00` is a write to
>   the joypad, so this is also how a game reads input.
> * `ADD SP,e8` / `LD HL,SP+e8`: `Z = 0`, `N = 0`, and `H`/`C` come from
>   `(SP & 0x0F) + (offset & 0x0F)` and `(SP & 0xFF) + (offset & 0xFF)`. Do **not**
>   reuse the 16-bit `ADD HL,rr` flag rule, and do not test the full 16-bit sum.
> * `ADD HL,rr` is the one 16-bit add that uses **bit 11** for `H` and leaves `Z` alone.
>   `INC rr`/`DEC rr` touch no flags whatsoever.
> * `0x08 LD (a16),SP` writes **two separate bytes** (`bus_write(a, sp & 0xFF)`,
>   `bus_write(a + 1, sp >> 8)`), not a 16-bit bus access.
> * Serial: route `FF01` -> `serial.sb` and `FF02` -> `serial.sc`, and handle the
>   `0x81` write path in `bus_write`. Reading `FF02` returns the stored value, so after
>   the shortcut a read gives `0x01` and `(sc & 0x80) == 0`.
> * `m05_serial_emits_byte` sets `gb->serial_out` to a temporary file, writes `SB = 'A'`
>   then `SC = 0x81`, and then reads the file back expecting `'A'`, `SC` bit 7 clear and
>   `IF` bit 3 raised. Three assertions, three lines of implementation.

---

## 2. Your task — 60 min

1. **Work out what is actually missing** (5 min). `grep` your `op_execute` for the
   opcode list above, or add a scratch test that executes each one and prints its name.
   Do not implement what you already have.
2. **The `LDH`/`(C)` forms** (15 min). Four opcodes, all two lines each.
3. **The `SP` arithmetic and `JP HL`/`LD SP,HL`** (20 min). Five opcodes. Write the
   `SP + e8` flag rule as its own helper and use it twice; write `JP HL` as
   `gb->cpu.pc = cpu_hl(&gb->cpu);`.
4. **`LD (a16),SP` and the 16-bit pair instructions** (10 min). If you took L13's
   option you already have the pairs; `0x08` is three lines.
5. **Serial** (15 min). Route `FF01`/`FF02` in `bus.c`, implement the `SC = 0x81`
   shortcut, and leave `serial_tick` empty with a `TODO(L30)` comment — the shortcut
   makes it unnecessary for now.
6. **Run the tests** (5 min):
   ```
   .\gb\build.cmd -Test m05
   ```
   All **eight** green.

---

## 3. Prove it — 10 min

```
.\gb\build.cmd -Test m05
```
```
   8 passed,    0 failed,    0 skipped
ALL GREEN
```
Then prove the whole instruction set is present with a scratch test that runs each
opcode once in a safe state and asserts no fault. You already did this for the CB page in
L14; do it for the base page now. If one opcode is missing you will see
`UNIMPLEMENTED at opcodes.c:NNN: opcode XX at PC=0100`, and that is the last time you
should see it before the gate.

Serial, from the command line — this is the moment `--serial -` starts paying:

```
.\gb\build.cmd
.\gb\build\gbemu.exe --rom <any ROM you have> --serial - --frames 60
```
You will not get a sensible verdict yet (the ROM needs L18's VBlank to make progress),
but if your serial path works you will see bytes appear rather than nothing at all.

```
.\gb\build.cmd -Test m0
```
Regression: `m00`-`m05` entirely green. The first red test should now be in `m06_*`.

---

## 4. If it fails

| Symptom | Cause | Find it |
| --- | --- | --- |
| `LDH` writes to a high address | you used `a8 | 0xFF00` with the wrong width, or forgot the `0xFF00` | mask the immediate to 8 bits first |
| `m05_serial_emits_byte` gets no character | the `SC` write path is not wired, or you handled `0x80` instead of `0x81` | `value == 0x81` exactly |
| The character arrives but `SC` still reads `0x81` | you emitted the byte without updating `sc` | set `sc = 0x01` |
| The character arrives and `SC` is right but `IF` bit 3 is clear | you called `gb_serial_byte` without `bus_request_interrupt` | both, together |
| `ADD SP,e8` sets `Z` when the result is 0 | you recomputed `Z` | `Z` is always 0 for this pair |
| `ADD SP,e8` computes `H` from bit 11 | you copied the `ADD HL,rr` rule | the low-byte nibble rule |
| `ADD HL,rr` clears `Z` | you went through a general flag helper | `Z` is unchanged; only `N`, `H`, `C` are written |
| `INC rr` clears `Z` | you reused the 8-bit `INC` path | 16-bit `INC`/`DEC` touch nothing |
| `0x08` writes only one byte | you used a 16-bit write | two 8-bit writes, low then high |

---

## 5. Done when

- [ ] All eight `m05_*` tests are green
- [ ] Your base-page sweep finds no `UNIMPLEMENTED` opcode except `STOP`
- [ ] `--serial -` prints bytes when a ROM writes to `SB`/`SC`
- [ ] `-Test m0` shows `m00`-`m05` fully green
- [ ] You can state the `ADD SP,e8` flag rule and why it differs from `ADD HL,rr`
- [ ] Commit message like `feat(cpu): LDH, SP arithmetic, and the serial port (L17)`

---

## 6. Optional, only if you have time

* `docs/04-ppu-and-peripherals.md` §4.5 — the serial section, including what a real
  4096-cycle transfer looks like and why the shortcut is nonetheless honest.
* Implement `0x10 STOP` as a two-byte instruction that halts. `cpu_instrs` explicitly
  skips it, but real games use it (and on CGB it is the speed switch), so noting the
  behaviour now costs two minutes.

Next: **[L18 — The clock, VBlank, and the boot state](L18-the-clock-vblank-and-the-boot-state.md)**
