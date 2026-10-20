# T10 — Everything together (encoder + both motors)

**What this test is about:** one program, one breadboard. The ESP32 reads the
encoder **and** drives both motors at the same time, and sends everything to the
Pi (or laptop) over USB. This is the program the Pi teammate will talk to.

**Wiring:** the **T10** tab of the wiring atlas
(`docs/wiring/index.html`). In short: T4 (encoder + chip) **plus** T8 (both drivers).
Every signal wire stays the same. The only new thing: a **3V3 rail** and a
**GND rail**, because the ESP32 does not have enough 3.3V / GND pins for everything.

**For your teammate:** [docs/PROTOCOL.md](../../docs/PROTOCOL.md) (version 2).
Same messages as T6 + T9 together; `hello` now says `"fw":"T10"`.

---

## Before you power on
- [ ] USB unplugged and motor power OFF while wiring.
- [ ] Rails marked with tape: **3V3** and **GND**. Only ESP32 **3V3** feeds the 3V3 rail.
- [ ] Before plugging in the chip and drivers: USB on, meter on the rails → **3.3 V** (not 5 V). USB off again.
- [ ] Encoder 5V comes from the ESP32 **5V** pin, not from a rail.
- [ ] Driver 2 **MS1 → 3V3 rail**, driver 1 MS1 → GND rail.
- [ ] Motors plugged in **before** motor power. Current limit ≈ 1 A (set in software, 700 mA).
- [ ] Power order: **USB first, motor power last.** Off: motor power first.

## 1. Upload
Open `tests\T10_all_together` → **Build** → **Upload**. Close the Serial Monitor.

## 2. Start the test tool (VS Code terminal, your COM number, e.g. COM7)
```
& "$env:USERPROFILE\.platformio\penv\Scripts\python.exe" "C:\Claude\Stepper Motors and Rotary Encoder\tools\serial_check.py" COM7
```
You should see `HELLO {'fw': 'T10', ...}`, then `hb` lines (encoder) and `MOTOR off` lines.
Now **motor power ON** → within 3 s the motor lines say `idle`.

## 3. Encoder alone
Type `z` + Enter (zero), then turn the encoder by hand → `pos` lines, the length changes.

## 4. Both at the same time (the real test)
1. `S` → both motors turn at 10 rpm (`running`).
2. **While they turn**, turn the encoder by hand → `pos` lines keep coming, length changes smoothly.
3. `P` → motors stop and hold. `X` → they unwind back to the start (`idle`, `turns 0.000`).
4. `diff` stays **0** the whole time, and at the end: `lost lines: 0`.

## 5. Finish
`X` (back to start) → motor power OFF → USB.

---

## Send me
- Did `pos` lines keep coming while the motors turned? Any jumps in length?
- `diff` always 0? `lost lines: 0`?
- Did Reset end exactly at the start marks?
