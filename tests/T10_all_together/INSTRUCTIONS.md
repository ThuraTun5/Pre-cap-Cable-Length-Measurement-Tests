# T10 — Everything together (encoder + both motors)

**What this test is about:** one program, one breadboard. The ESP32 reads the
encoder **and** drives both motors at the same time, and sends everything to the
Pi (or laptop) over USB. This is the program the Pi teammate will talk to.

**Wiring:** wiring atlas (`docs/wiring/index.html`), three tabs, wire them in order:
**T10 ①** chip + encoder (exactly T4) → **T10 ②** driver 1 → **T10 ③** driver 2.
Every signal wire is the same as in T4 / T7 / T8. New: a **GND rail** (from T4) and a
**3V3 rail** for the drivers, because the ESP32 has only 4 × 3.3V and 7 × GND pins.

**Pin names you need** (hold the parts like this):
- **74LVC245:** notch at the top. Left legs top → bottom = 1–10, right legs top → bottom = 20–11.
- **TMC2209:** the word TOP readable, potentiometer at the top.
  Left column: EN, MS1, MS2, PDN, PDN (leave empty), CLK, STEP, DIR.
  Right column: VM, GND, A2, A1, B1, B2, VDD, GND.

**ESP32 power pins (every 3.3V pin is the same, every GND pin is the same):**

| ESP32 pin | Goes to |
|---|---|
| 5V | encoder 5V |
| 3.3V #1 | chip pin 20 (and pin 1) |
| 3.3V #2 | R2 10 kΩ → EN wire |
| 3.3V #3 | **3V3 rail** → VDD of driver 1 and driver 2 |
| 3.3V #4 | driver 2 MS1 |
| GND #1 | chip pin 19 (OE) |
| GND #2 | **GND rail** → chip pins 5–10, encoder G, logic GND (bottom-right pin) of both drivers |
| GND #3 | driver 1 MS1 + MS2 + CLK (short jumpers between them) |
| GND #4 | driver 2 MS2 + CLK |

**For your teammate:** [docs/PROTOCOL.md](../../docs/PROTOCOL.md) (version 2).
Same messages as T6 + T9 together; `hello` now says `"fw":"T10"`.

---

## Before you power on
- [ ] USB unplugged and motor power OFF while wiring.
- [ ] Rails marked with tape: **3V3** and **GND**.
- [ ] Before plugging in the chip and drivers: USB on, meter between the rails → **3.3 V** (not 5 V). USB off again.
- [ ] Encoder 5V comes from the ESP32 **5V** pin, not from a rail.
- [ ] Driver 2 **MS1 → 3.3V**, driver 1 MS1 → GND.
- [ ] The 5th left pin of each driver (second PDN) is empty.
- [ ] Motor supply (VM, power GND) goes only to the drivers, never to the rails.
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
