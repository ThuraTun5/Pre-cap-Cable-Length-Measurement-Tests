# T08 — Two drivers, two motors in sync

**Do T07 first.** T08 adds the second driver and motor to the working T07 setup.

**What this test is about:** both motors turn together, at exactly the same
speed. Motor 2 turns the **opposite** way (it faces motor 1 at the other end of
the cable), so the cable turns as one piece. The ESP32 shows the step
difference between the two motors, which must be **0** when they stop.

**Wiring drawing:** [Wiring Atlas](../../docs/wiring/index.html) → tab **T8 Two drivers**.

The same 3 rules as T07: never plug or unplug a motor while motor power is on;
USB first, motor power last (off: motor power first); current limit about 1 A.

---

## 1. Add driver 2 (USB unplugged, motor power OFF)

Find motor 2's coil pairs with the meter first (about 4 Ω = one pair), as in T07.

**Driver 2 is wired like driver 1, with 3 differences:**

| Driver 2 pin | To | Different from driver 1? |
|---|---|---|
| EN | driver 1 **EN** (same wire / same row) | shared |
| MS1 | ESP32 **3.3V** | **YES** (driver 1: GND). This gives it address 1 |
| MS2 | ESP32 **GND** | |
| PDN (4th pin) | driver 1 **PDN (4th pin)** (same row) | shared |
| PDN (5th pin) | nothing | |
| CLK | ESP32 **GND** | |
| STEP | ESP32 **27** | **YES** (driver 1: 25) |
| DIR | ESP32 **14** | **YES** (driver 1: 26) |
| VM, GND (top) | driver 1 **VM**, **GND** (same supply) | shared |
| A2 A1 B1 B2 | motor 2 coil pairs | |
| VDD | ESP32 **3.3V** | |
| GND (bottom) | ESP32 **GND** | |

- Second **100 µF** capacitor across driver 2's VM and GND (stripe → GND).
- Running out of ESP32 GND or 3.3V pins? Several wires can go into the **same breadboard row** as one ESP32 pin.

## 2. Run it
1. USB → open `tests\T08_two_motors` → **Build** → **Upload** → **Serial Monitor**.
2. **Motor power ON.** Type `i` → both lines must say **UART OK**:
   ```
   Driver 1: UART OK (address 0, ...)
   Driver 2: UART OK (address 1, ...)
   ```
3. Place the motors **facing each other** (shafts pointing at each other, like in the jig).
   Stick a small tape flag on each shaft.
4. `e`, then `f`. Look **from the side**: both flags must move the **same way**
   (for example both tops moving toward you). If they move against each other:
   `s`, then `m` (mirror), then `f` again. The setting is saved.
5. Let them run about 30 s → `s`. The last line must say **difference 0**.
6. `g` → both go back to their start marks. `x`, then motor power OFF.

## You should see
| Check | Expected |
|---|---|
| `i` | Driver 1 **and** driver 2 UART OK |
| `f` | both turn smoothly, quietly, together |
| after `s` | `difference 0 steps` |
| `g` | both back at their tape marks |

## Send me
- The two lines after `i`.
- Did you need `m`?
- The `difference` after stopping.
