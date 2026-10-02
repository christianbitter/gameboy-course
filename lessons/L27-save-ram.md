# L27 — Save RAM

**Time** — theory ~20 min | coding ~50 min | verify ~20 min
**You will end with** — a save file that survives a restart, in a format other emulators
can read. AHA: *something I did yesterday is still there today.*
**Tests that must go green** — `m08_battery_save_roundtrip`
**Depends on** — L26 (`ram_enabled`, RAM banking). You will edit `gb/src/cart.c`.

---

## 1. Read this — the whole theory for today

### The save file *is* the cartridge RAM

There is no save format. Battery-backed cartridge RAM is the save: the game writes its
state into `A000-BFFF` and the battery keeps the bytes alive. An emulator persists that
same RAM to a file, and the least surprising file is a **raw dump of the RAM banks**, one
after another, in bank order:

```
<rom name>.sav   ==   byte-for-byte the contents of the cartridge RAM
```

That is why emulators can share save files and why the community settled on it. Do not
invent a header, a version number or a checksum: a raw dump is what every other emulator
and every save editor expects.

### When to load, and why it must be early

Games do not trust the save area. The usual pattern is:

1. look for a magic sequence at a fixed address in RAM (e.g. `0x5A 0xA5`),
2. if it is absent, treat the save as empty and initialise it,
3. if it is present, load the real save data.

So the RAM must be populated **before the game reads it for the first time** — which means
inside `cart_load`, not later when the first frame runs. That is why the save path is
derived there too:

```
cart_load(gb, data, size, path):
    ... parse the header, allocate ram ...
    derive "<path with .gb replaced by .sav>" into gb->save_path
    if that file exists: read it into gb->cart.ram (up to ram_size bytes)
cart_save(gb):
    if gb->save_path is set: write gb->cart.ram, whole buffer, to it
```

`m08_battery_save_roundtrip` depends on exactly this: it saves with one machine, destroys
it, builds a **second** machine from the same ROM file path, and expects the bytes to be
there. There is no other point in the API where a fresh machine could pick the save up.

### When to write

Three triggers, and a sane implementation has all three:

| Trigger | Where | Why |
| --- | --- | --- |
| a dirty flag | `gb->cart.ram_dirty` set by any RAM write | avoids rewriting a file that did not change |
| process exit | `main.c` already calls `cart_save` when `ram_dirty && battery` | the common case |
| on demand | a future `--save` flag, or L32's save states | manual control while debugging |

`main.c` has had that exit hook since L01 and it does nothing until today: `battery` is
false for most cartridges, and `ram_dirty` is never set until you write RAM.

A game that saves but whose RAM is *not* marked dirty will lose the save when you close
the emulator, and it will look like a bug in the game. Set the flag on every write, even
when the value is identical.

### The battery flag, and the RTC

`gb->cart.battery` comes from the cartridge type (`0x03`, `0x06`, `0x09`, `0x0D`, `0x0F`,
`0x10`, `0x13`, `0x1B`, `0x1E`, `0x22`, `0xFF`). It matters for two things: whether a
`.sav` is expected at all, and whether a game's clock should be persisted.

MBC3 cartridges with an RTC also have a real-time clock that must survive a power cycle,
which means the `.sav` convention usually grows an extra 44-byte footer holding the
latched RTC registers and a timestamp. That is a *convention*, not a format: emulators
disagree about it, and no test here requires it. Note it in `NOTES.md` for L28 and move
on — the test only requires the RAM bytes.

### The dirty flag and `cart_unload`

`ram_dirty` lives in `cart_t` and `cart_unload` frees `ram`. Do not free the RAM before
a final save: `main.c` saves *before* `gb_destroy`, which is the right order — but if you
add an early-exit path with `exit()`, nothing will have saved. Prefer returning through
`main`'s exit path over calling `exit()` from inside the emulator.

> **What matters for the code you are about to write**
>
> * `cart_load` derives `<path>.sav` (replace a trailing `.gb` with `.sav`, otherwise
>   append `.sav`) and stores it in **`gb->save_path`**, then loads it if it exists. The
>   test's second machine relies on this and nothing else.
> * Load with `fread(ram, 1, ram_size, f)` and accept a **short file** (an older save from
>   a smaller cartridge, or a new one being created). Do not fail a load because the file
>   is the wrong size; clamp it.
> * `cart_save` writes **the whole RAM buffer**, not just the pages you touched, so the
>   file is always a complete image.
> * Set `gb->cart.ram_dirty = true` on **every** RAM write, even when the value is
>   unchanged. A game that writes the same bytes twice is still saving.
> * `m08_battery_save_roundtrip` checks `gb->cart.ram_size == 32 * 1024` for header code
>   `0x03`, so the allocation must come from the header table and not from the file size.
> * The test writes into `gb/build/` and deletes its `.sav` afterwards. If the file cannot
>   be created, the test fails with the round-trip assertion; check that `gb/build/`
>   exists (the build script creates it).
> * Do **not** create a `.sav` for a cartridge with no battery. It is harmless but noisy,
>   and it will confuse you later when a homebrew ROM leaves files behind.

---

## 2. Your task — 50 min

Work in `gb/src/cart.c`.

1. **The save path** (10 min). In `cart_load`, build `<path>` with a `.gb` suffix replaced
   by `.sav` (or `.sav` appended) into `gb->save_path`. `gb.c`'s `make_save_path` is a
   static helper doing exactly this for a different caller — read it, then write your own
   or reuse the logic.
2. **Load it** (15 min). If the file opens, `fread` up to `ram_size` bytes into
   `gb->cart.ram`. A short read is fine; a missing file is normal. Only load when
   `battery` is set, so a non-battery cart never inherits a stale file.
3. **The dirty flag** (10 min). Set `ram_dirty` in the `A000-BFFF` write path you built in
   L26 — one line, in the branch where the write actually lands in RAM (not when RAM is
   disabled).
4. **`cart_save`** (10 min). Write the whole RAM buffer to `gb->save_path` if it is set.
   Clear `ram_dirty` on success. Report a failure on stderr rather than silently.
5. **Run the test** (5 min):
   ```
   .\gb\build.cmd -Test m08
   ```
   `m08_battery_save_roundtrip` green. The MBC3/MBC5 tests are L28.

---

## 3. Prove it — 20 min

```
.\gb\build.cmd -Test m08
```
```
[       OK ] m08_battery_save_roundtrip
...
```
Then do it for real, which is the whole point:

```
.\gb\build\gbemu.exe --rom <a game with a save>.gb --frames 3600
dir *.sav
.\gb\build\gbemu.exe --rom <that game>.gb --frames 60
```

1. Play long enough for the game to reach a save point (or write the save from the game's
   own menu). With no window until L34, the practical version is: run the game for a few
   thousand frames, exit, and check that a `.sav` appeared next to the ROM.
2. Check the file's size: it must equal the cartridge's RAM size from `--info`
   (8 KiB, 32 KiB, ...). A 0-byte or 512-byte file means the write path is wrong.
3. Reload and confirm the game does not offer "new game" only — many games show a
   continue option as soon as the RAM has a valid magic sequence.

A third check worth doing, because it is what "raw dump" is for: open the `.sav` in the
hex editor extension (VS Code has one configured for `.sav`) and compare a few bytes with
what the game wrote. A save that is mostly `0xFF` is usually a save the game never wrote,
not a broken save.

---

## 4. If it fails

| Symptom | Cause | Find it |
| --- | --- | --- |
| No `.sav` appears at all | `gb->save_path` is empty, or `ram_dirty` was never set | print both before the save call |
| `.sav` is 0 bytes | `ram_size` is 0: you allocated RAM from the file size, not the header | `cart_ram_size_bytes` |
| The round-trip test fails on the second machine | `cart_load` does not load the save, or derives a different path | the path must come from `cart_load`'s `path` argument |
| The file is written but the game still starts a new game | the save is loaded *after* the game's first read | load inside `cart_load` |
| The save is written on every frame | you call `cart_save` in the emulation loop | save on exit and on demand, not per frame |
| The bytes are there but shifted by a bank | you wrote only the current RAM bank | write the whole buffer, bank 0 first |
| A non-battery homebrew ROM leaves a `.sav` behind | you do not check `battery` | gate both load and save on it |
| The emulator crashes on exit | you freed `cart.ram` before `cart_save` | save first, then destroy |

---

## 5. Done when

- [ ] `m08_battery_save_roundtrip` is green
- [ ] A real game leaves a `.sav` whose size matches `--info`'s RAM size
- [ ] The second run of that game can see the save
- [ ] `-Test m0` shows no regression
- [ ] You can explain why the save must be loaded before the ROM's first read
- [ ] You can explain why the `.sav` has no header
- [ ] Commit message like `feat(cart): battery RAM persistence as a raw .sav (L27)`

---

## 6. Optional, only if you have time

* `docs/03-memory-and-cartridge.md` §3.5 — the battery flag for every cartridge type, and
  the RAM size table you are already using.
* Add a `--save` and `--no-save` flag pair so you can run a game without touching its
  save file. Ten minutes, and it stops you from corrupting a save while debugging L29's
  timing work.

Next: **[L28 — MBC3, MBC5 and the RTC](L28-mbc3-mbc5-and-the-rtc.md)**
