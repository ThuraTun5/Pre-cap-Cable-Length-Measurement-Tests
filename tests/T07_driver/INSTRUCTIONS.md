# T07 — One driver + one motor

**What this test is about:** the ESP32 talks to the TMC2209 driver, sets a safe
motor current, and turns **one** motor slowly forward, backward and back to its
start. The program keeps the motor OFF until the driver has answered.

**Wiring drawing:** [Wiring Atlas](../../docs/wiring/index.html) → tab **T7 Driver**.
Build this on its own. The encoder can stay wired, but it isn't used here.

---

## ⚠️ The 3 rules for this test
1. **Never plug or unplug the motor while motor power is ON.** It destroys the driver.
2. **Motor power ON last, OFF first.** Order: USB → motor power ON → … → motor power OFF → USB.
3. **No fuse yet?** Then your supply must have a **current limit**: set it to about **1 A**.
   If it has no current limit, wait for the fuse.

---

## 1. Find the motor coil pairs (meter on Ω)
The motor has 4 wires = 2 coils. Measure between wires:
- About **4 Ω** → the two wires are one coil (a "pair").
- **No reading (OL)** → different coils.

One pair goes to driver pins **A1 + A2**, the other pair to **B1 + B2**.
(Which wire of a pair goes to "1" or "2" only changes the turning direction.)

## 2. Wire it (USB unplugged, motor power OFF)

**Driver, left side** (EN, MS1, MS2, PDN, PDN, CLK, STEP, DIR):
| Driver pin | To |
|---|---|
| EN | ESP32 **13**, and a **10 kΩ** resistor from EN to ESP32 **3V3** |
| MS1 | ESP32 **GND** |
| MS2 | ESP32 **GND** |
| PDN (4th pin) | ESP32 **4**, and a **1 kΩ** resistor from this pin to ESP32 **17** |
| PDN (5th pin) | **nothing** |
| CLK | ESP32 **GND** |
| STEP | ESP32 **25** |
| DIR | ESP32 **26** |

**Driver, right side** (VM, GND, A2, A1, B1, B2, VDD, GND):
| Driver pin | To |
|---|---|
| VM | Supply **+** (24 V) |
| GND (next to VM) | Supply **−** |
| A2, A1 | motor coil pair 1 |
| B1, B2 | motor coil pair 2 |
| VDD | ESP32 **3.3V** |
| GND (bottom) | ESP32 **GND** |

**Capacitor 100 µF:** across **VM and GND**, close to the driver. **Stripe (−) → GND.**

**Check before power:**
- [ ] Driver orientation: VM and GND are on the **right** side as in the drawing.
- [ ] VDD goes to **3.3V**, not to VM.
- [ ] Capacitor stripe on GND.
- [ ] Motor plugged in **now**.

## 3. Run it
1. Plug in USB. Open `tests\T07_driver` → **Build** → **Upload** → **Serial Monitor**.
   It says `Motor is OFF`.
2. **Motor power ON** (24 V).
3. Type `i` + Enter → you should see:
   ```
   UART OK: TMC2209 version 0x21. Set: 700 mA RMS, 16 microsteps, StealthChop.
   ```
   `UART FAILED`? Motor power on? PDN wire on the 4th pin? Common GND? Tell me.
4. `e` → motor **ON**: the shaft is now hard to turn by hand. Blue LED on.
5. `f` → turns forward slowly (30 rpm). `s` → stops. `b` → backward. `s`.
6. Put a tape mark on the shaft. `f`, wait a few turns, `s`, then `g` → it goes back
   to **exactly the same mark**.
7. `x` → motor OFF (shaft free). Then **motor power OFF**, then USB.

## You should see
| Check | Expected |
|---|---|
| `i` | `UART OK … version 0x21` |
| `e` | shaft holds, quiet hum or silent |
| `f` / `b` | smooth, quiet turning, position counts up / down |
| `g` | back to the tape mark |
| Driver after 2 min | warm, not too hot to touch |
| Motor after 2 min | warm is normal |

**Stop at once (motor power OFF)** if the motor only shakes, buzzes loudly, or
anything smells or gets very hot. Tell me what you saw.

## Send me
- The line after `i`.
- Did `g` return to the mark?
- Your supply: did it have a current limit, and what current did it show while turning?
