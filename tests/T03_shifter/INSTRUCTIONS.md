# T03 — 74LVC245 Level Shifter

**Goal:** prove the SN74LVC245AN turns 5 V signals into safe 3.3 V signals for
the ESP32, first with the meter (Part A), then with the ESP32 reading three
channels exactly as the encoder will use them in T4 (Part B).

**Why it matters:** the encoder outputs about 5 V. An ESP32 pin is only rated to
3.6 V (absolute maximum). Without this chip, the encoder would slowly or
instantly damage the ESP32.

**Wiring diagrams:** [Wiring Atlas](../../docs/wiring/index.html) → tabs **T3 Part A**
and **T3 Part B**.

**Risk level:** low. Only 3.3 V and 5 V from the ESP32's USB power. The main
risk is putting 5 V on the wrong pin.

---

## What you need
- ESP32 on USB, breadboard, jumper wires
- SN74LVC245AN (20-pin DIP), one **100 nF** ceramic capacitor (marked "104")
- Multimeter

## Chip orientation (both parts)
- Push the chip across the **centre gap** of the breadboard.
- The **notch** (half-moon) or dot marks the top. **Pin 1 = top-left.**
  Pins count down the left side 1 → 10, then up the right side 11 → 20.
- Pin 1 DIR · 2–9 A1–A8 · **10 GND** · 11–18 B8–B1 · 19 OE · **20 VCC**

---

## Part A — check with the meter

### Before you power on
- [ ] USB **unplugged** while wiring.
- [ ] **Pin 20 (VCC) → 3V3 rail** (from the ESP32 **3V3** pin). Not 5V!
- [ ] **Pin 10 (GND) → GND rail** (from an ESP32 **GND** pin).
- [ ] **Pin 1 (DIR) → 3V3 rail** (HIGH = signals go A → B).
- [ ] **Pin 19 (OE) → GND rail** (LOW = outputs on).
- [ ] 100 nF capacitor right at the chip: one leg on the pin-20 row, the other
      on a GND row (it has no polarity).
- [ ] **Pins 3–9 (A2–A8) → GND rail.** Unused inputs must never float.
- [ ] Test jumper: ESP32 **5V** pin → **pin 2 (A1)**.
- [ ] **Pins 11–18 (B side)** have nothing connected except the meter probe on pin 18.

### Steps and what you should see
| Step | Measure (COM on GND) | Expected |
|---|---|---|
| 1. Plug in USB | Pin 20 (VCC) | 3.2–3.35 V |
| 2. | Pin 2 (A1, 5 V jumper) | 4.7–5.1 V |
| 3. | **Pin 18 (B1)** | **3.2–3.35 V**, never above 3.4 V |
| 4. Move the A1 jumper from 5V to GND (USB may stay on: it's only a signal jumper) | Pin 18 (B1) | below 0.1 V |
| 5. | Touch the chip | Cold |

**Stop and tell me** if pin 18 ever reads above 3.4 V, or if the chip gets warm.

---

## Part B — the ESP32 reads the shifted signals

### B1. Upload (USB only, the chip may stay wired)
1. Open `tests/T03_shifter` in VS Code → **Build ✓** → **Upload →** → **Serial Monitor 🔌**.

### B2. Wiring changes from Part A (USB unplugged)
- [ ] Remove the meter probe from pin 18, and remove the GND wires from **pins 3 and 4**.
- [ ] Make a short **5V rail**: ESP32 **5V** pin → one free breadboard row.
- [ ] **Pin 18 (B1) → GPIO35**, **pin 17 (B2) → GPIO34**, **pin 16 (B3) → GPIO32**.
      All three are in the ESP32's left pin row, next to each other.
- [ ] Test jumpers (start position): **A1 (pin 2) → 5V**, **A2 (pin 3) → 5V**, **A3 (pin 4) → GND**.
- [ ] Pins 5–9 (A4–A8) still go to GND. Pins 11–15 (B4–B8) stay empty.
- [ ] **No B pin and no ESP32 pin touches the 5V rail.**

### B3. Steps
1. Plug in USB, open the serial monitor, press **EN/RST**.
2. You should see:
   ```
   === T03 74LVC245 level shifter test ===
     A1 -> B1 -> GPIO35 : HIGH
     A2 -> B2 -> GPIO34 : HIGH
     A3 -> B3 -> GPIO32 : LOW
   ```
   and the RGB LED **yellow** (red = A1 HIGH + green = A2 HIGH).
3. Move **one jumper at a time** to the other rail. You can do this with USB on:
   these are only 5 V signal jumpers. Each move must print **exactly one** line,
   for example:
   ```
   A3 -> B3 -> GPIO32 : LOW  -> HIGH
   ```
4. Try all 8 combinations, if you like. The RGB colour follows:
   A1 = red, A2 = green, A3 = blue (all HIGH = white, all LOW = off).
5. Type `p` + Enter at any time to print all three levels.

### You should see
| Check | Expected | If not |
|---|---|---|
| Start levels | HIGH, HIGH, LOW + yellow | Check the B → GPIO wires |
| Each jumper move | Exactly 1 line | Several lines = bouncing contact; fine if it settles. Tell me if it keeps flipping. |
| `WARNING … changes in 1 s` | Never (except while a jumper hangs loose in the air) | A wire is loose, or an A input is floating |
| Blue LED | Blinks 1× per second | Program not running |

---

## Please report back
1. Part A: the voltages at pins 20, 2 and 18 (both jumper positions).
2. Part B: did each jumper move print exactly one correct line? Any WARNING lines?
3. Anything that got warm.
