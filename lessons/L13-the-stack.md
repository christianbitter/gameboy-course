# L13 — The stack

**Time** — theory ~20 min | coding ~55 min | verify ~15 min
**You will end with** — a working stack: `PUSH`/`POP`, `CALL`/`RET`, and the eight
`RST` vectors. AHA: *subroutines work, so my CPU can run real code structure.*
**Tests that must go green** — `m04_call_ret_stack`, `m04_push_pop_order`,
`m04_rst_vectors` (plus L12's two, so all five non-CB `m04_*` are green)
**Depends on** — L06 (`JP`), L12 (conditional jumps). You will edit `gb/src/opcodes.c`.

---

## 1. Read this — the whole theory for today

### The stack grows downwards

`SP` points at the top of the stack. Pushing **decrements** it; popping
**increments** it. There is no boundary and no protection: the stack can run off the
end of WRAM and the hardware will happily write to whatever is there.

A push stores two bytes, and the order matters:

```
PUSH rr:   bus_write(--SP, high byte);
           bus_write(--SP, low  byte);

POP rr:    lo = bus_read(SP++);
           hi = bus_read(SP++);
```

Why high first? Because after both writes, `SP` points at the low byte, so a 16-bit
little-endian read at `SP` reconstructs the value. `m04_push_pop_order` checks the raw
memory (`mem[0xFFFD] == 0x12`, `mem[0xFFFC] == 0x34` for `PUSH BC` with `BC = 0x1234`)
precisely so you cannot get away with the reverse order.

### The opcodes

| Opcode | Instruction | T-cycles | Notes |
| --- | --- | --- | --- |
| `0xC5` `0xD5` `0xE5` `0xF5` | `PUSH BC` / `DE` / `HL` / `AF` | 16 | |
| `0xC1` `0xD1` `0xE1` `0xF1` | `POP BC` / `DE` / `HL` / `AF` | 12 | |
| `0xCD` | `CALL a16` | 24 | |
| `0xC4` `0xCC` `0xD4` `0xDC` | `CALL NZ/Z/NC/C,a16` | 24 taken / **12 not taken** | |
| `0xC9` | `RET` | 16 | |
| `0xC0` `0xC8` `0xD0` `0xD8` | `RET NZ/Z/NC/C` | 20 taken / **8 not taken** | |
| `0xD9` | `RETI` | 16 | pop `PC`, then enable interrupts (L15) |
| `0xC7` `0xCF` `0xD7` `0xDF` `0xE7` `0xEF` `0xF7` `0xFF` | `RST 00H` ... `RST 38H` | 16 | push, then jump to `0x0000 + n*8` |

`PUSH` is 16 T (two writes) and `POP` is 12 T (one read plus `SP` arithmetic). Note
that `POP` is *cheaper* than `PUSH` — unusual, and real.

### `POP AF` cannot restore the low nibble of `F`

`F`'s low four bits do not exist. `POP AF` writes `F = popped & 0xF0`. The test pushes
`0xFFFF` and expects `A = 0xFF`, `F = 0xF0`. If you assign the raw byte you will get
`F = 0xFF` and every later flag test that compares against a literal will fail.

### `CALL` is "push the next address, then jump"

```
CALL a16:   u16 target = cpu_fetch16(gb);     /* PC is now past the instruction */
            push(gb->cpu.pc);                 /* the return address            */
            gb->cpu.pc = target;
```

The return address is whatever `PC` is after the operand fetch — you never compute it
yourself, which is why the order above is the whole implementation. `RET` is the
mirror: pop into `PC`. `RST n` is the same trick with a fixed 1-byte instruction, so
the pushed address is `PC` after fetching one byte (e.g. `RST 08H` at `0x0100` pushes
`0x0101` and jumps to `0x0008`).

### The `RST` targets

| Opcode | `C7` | `CF` | `D7` | `DF` | `E7` | `EF` | `F7` | `FF` |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Target | `0x0000` | `0x0008` | `0x0010` | `0x0018` | `0x0020` | `0x0028` | `0x0030` | `0x0038` |

These are the interrupt vectors' neighbours: `RST 38H` at `0x0038` sits just below
`VBlank` at `0x0040`, which is how a program installs a handler table.

### The 16-bit pair instructions (not tested today, needed soon)

| Opcode | Instruction | T-cycles | Flags |
| --- | --- | --- | --- |
| `0x03` `0x13` `0x23` `0x33` | `INC BC` / `DE` / `HL` / `SP` | 8 | **none at all** |
| `0x0B` `0x1B` `0x2B` `0x3B` | `DEC BC` / `DE` / `HL` / `SP` | 8 | **none at all** |
| `0x09` `0x19` `0x29` `0x39` | `ADD HL,rr` | 8 | `N=0`, `H` from bit 11, `C` from bit 16, `Z` **unchanged** |

`INC rr`/`DEC rr` touching no flags is easy to get wrong by reusing your 8-bit `INC`.
`ADD HL,rr` is the only 16-bit arithmetic a game performs constantly (pointer walks);
its `H` comes from bit 11, not bit 3, and it leaves `Z` alone. No unit test covers
these three families — `cpu_instrs` at L17 does — so treat them as today's debt and
tick them off.

> **What matters for the code you are about to write**
>
> * `PUSH`: `bus_write(gb, --gb->cpu.sp, hi); bus_write(gb, --gb->cpu.sp, lo);`
>   **High byte first.** Pre-decrement, not post.
> * `POP`: `lo = bus_read(gb, gb->cpu.sp++); hi = bus_read(gb, gb->cpu.sp++);`
>   So a 16-bit value is reconstructed low-first, which is why push is high-first.
> * `POP AF` -> `cpu_set_af(&gb->cpu, popped)` uses the provided setter, which masks
>   `F` to `0xF0` for you. Do not assign `f` directly.
> * `CALL cc` fetches the address **before** testing the condition, so the not-taken
>   path still consumes three bytes and costs 12 T. Same shape as `JP cc` in L12.
> * `CALL`/`RST` push `gb->cpu.pc` *after* the operand fetch. Do not add 2 or 3.
> * `RETI`'s stack behaviour is identical to `RET` today; leave the interrupt-enable
>   side as a `TODO(L15)` comment so you do not forget it.
> * `m04_call_ret_stack` builds its own ROM with a `RET` at `0x0150`, because writes to
>   ROM are ignored — you cannot patch a ROM at runtime.

---

## 2. Your task — 55 min

Work in `gb/src/opcodes.c`.

1. **`PUSH`/`POP`** (20 min). Two helpers (`push16`, `pop16`) plus the eight opcodes.
   Derive the target pair from `op >> 4`: `0xC`->BC, `0xD`->DE, `0xE`->HL, `0xF`->AF.
   Use `cpu_bc`/`cpu_set_bc` and friends so the `AF` masking comes for free.
2. **`CALL` and `RET`** (20 min). `0xCD`, `0xC9`, plus the four conditional forms of
   each. Reuse L12's condition helper — the four conditions are the same table.
3. **`RST`** (10 min). One helper taking a target, eight opcodes. `target = op & 0x38`
   gets you all eight from the opcode itself.
4. **The 16-bit pairs** (5 min if you take the option below; otherwise record the debt
   in `NOTES.md`).
5. **Run the tests** (5 min):
   ```
   .\gb\build.cmd -Test m04
   ```
   Five green (`m04_call_ret_stack`, `m04_push_pop_order`, `m04_rst_vectors` plus
   L12's two), two red (`m04_cb_*` — L14).

---

## 3. Prove it — 15 min

```
.\gb\build.cmd -Test m04
```
```
[       OK ] m04_call_ret_stack
[  FAILED  ] m04_cb_bit_res_set      <- L14
[  FAILED  ] m04_cb_rotates          <- L14
[       OK ] m04_push_pop_order
[       OK ] m04_rst_vectors
...
   5 passed,    2 failed,    0 skipped
```
Then build a program by hand — this is the reward for the lesson. Write a scratch test
with:

```
0100: CD 08 01   CALL 0x0108
0103: 3E 42      LD A,0x42      <- returns here
0105: 00         NOP
0106: 76         HALT
0108: 3C         INC A          <- the "subroutine"
0109: C9         RET
```

Step it five times and confirm: after `CALL`, `SP = 0xFFFC` and `PC = 0x0108`; after
`RET`, `PC = 0x0103` and `SP = 0xFFFE`; then `A` becomes `0x42`. You just called and
returned from a subroutine on a CPU you wrote. Add it to `NOTES.md` as your first
reusable test program.

```
.\gb\build.cmd -Test m0
```
Regression: `m00`-`m04` green except L14's two.

---

## 4. If it fails

| Symptom | Cause | Find it |
| --- | --- | --- |
| `m04_push_pop_order` sees `mem[0xFFFD] == 0x34` | you pushed low byte first | high byte goes to `SP-1` |
| `SP` ends up `0xFFFE` after a `PUSH` | you used post-increment/post-decrement the wrong way round | `--SP` twice for push, `SP++` twice for pop |
| `m04_call_ret_stack` returns to `0x0100` or `0x0150` | you pushed the address *of* the instruction, or the jump target | push `gb->cpu.pc` after the fetch |
| `RET` returns to the right place but `SP` is wrong | you popped `PC` but not both bytes, or incremented before reading | pop low then high, incrementing as you go |
| `POP AF` yields `F = 0xFF` | direct assignment instead of the masking setter | `cpu_set_af` masks for you |
| `m04_rst_vectors` all land at `0x0000` | you used the raw opcode instead of `op & 0x38` | `0xCF & 0x38 = 0x08` |
| `CALL cc` not taken leaves `PC = 0x0150` | you assigned `PC` before testing | fetch, test, then maybe assign |

---

## 5. Done when

- [ ] Five `m04_*` tests green, two red, and you know the reds are the CB page
- [ ] Your hand-written `CALL`/`RET` program produces `A = 0x42` and `SP = 0xFFFE`
- [ ] `-Test m0` shows no regression
- [ ] You can state, without looking, which byte a `PUSH` writes first and why
- [ ] `NOTES.md` records the 16-bit pair instructions as L17 debt (or you implemented them)
- [ ] Commit message like `feat(cpu): the stack, CALL/RET, and the RST vectors (L13)`

---

## 6. Optional, only if you have time

* `docs/02-cpu.md` §2.5 — the stack section, including `LD (a16),SP` (`0x08`, 20 T) and
  `LD SP,HL` (`0xF9`, 8 T), which are also cheap to add now.
* Implement `ADD HL,rr` and the 16-bit `INC`/`DEC` pairs. They are 12 opcodes, all
  8 T, they take ten minutes, and every game uses them constantly.

Next: **[L14 — The CB page](L14-the-cb-page.md)**
