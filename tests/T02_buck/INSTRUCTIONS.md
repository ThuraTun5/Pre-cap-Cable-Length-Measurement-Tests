# T02 — Buck Converter → 5.0 V

**Goal:** set the buck converter to exactly 5.0 V (Part A), then prove it can
power the ESP32 on its own while Wi-Fi is busy (Part B).

**Why it matters:** in the final jig the ESP32 is powered from this converter,
not from USB. A wrong voltage here destroys the ESP32 instantly. That's why we
set it alone, with only the meter connected.

**Wiring diagrams:** [Wiring Atlas](../../docs/wiring/index.html) → tabs **T2 Buck**
(Part A) and **T2 Part B**.

**Risk level:** low to medium. 12 V is safe to touch, but a short circuit or
reversed polarity can damage the module or the ESP32.

---

## What you need
- Bench supply set to **12 V** (not 24 V for this test)
- The buck module, multimeter, small screwdriver
- Red/black wire (24 AWG, included with the buck kit)
- Fuse holder + **1 A** fuse **if your supply has no current-limit knob**
- For Part B: the ESP32 and 2 jumper wires

> **Note on the module:** the input/output points are **solder pads**
> (IN+, IN−, OUT+, OUT−), not screw terminals. Solder short wires to them.
> Crocodile clips are OK for a quick test, but they slip easily and can short
> two pads. If you use clips, keep them at an angle away from each other.

---

## Part A — set 5.0 V (nothing but the meter on the output)

### Before you power on
- [ ] Supply **OFF**. Set it to **12 V**. If it has a current limit, set **0.3 A**.
- [ ] Supply **+ (red)** → (fuse) → **IN+**. Supply **− (black)** → **IN−**.
      Check both twice. The marking is printed next to each pad.
- [ ] Meter on **DC V** (20 V range). Red probe → **OUT+**, black probe → **OUT−**.
- [ ] **Nothing else** on OUT+ / OUT−.
- [ ] The module lies on a non-conductive surface.

### Steps
1. Switch the supply **ON**. Watch the meter **straight away**.
   - A new module can come set to any voltage. Seeing 10–12 V at first is normal.
   - Smoke, a smell, or the supply's current limit lighting up → **switch OFF**
     and recheck the polarity.
2. Turn the blue trimmer screw **slowly**. On most modules **clockwise = higher**,
   but trust the meter, not the direction. The trimmer has many turns
   (about 15–25), so the voltage changes slowly. Keep turning.
3. Stop at **5.00 V** (accept 4.95–5.05 V).
4. Switch the supply **OFF**, wait 5 s, switch **ON** again. The meter must show
   the same 5.0 V. (This checks that the setting stays.)
5. *(Optional)* Set the supply to **24 V** and read the output again. It should
   still be 5.0 V. Then go back to 12 V.

### You should see
| Check | Expected |
|---|---|
| Output after adjusting | 4.95–5.05 V |
| After OFF → ON | Same value (± 0.02 V) |
| Supply current (no load) | Very small, a few mA (≈0.01 A on the supply display) |
| Module temperature | Cold |

---

## Part B — the ESP32 as a real load

### B1. Upload the program (USB only, buck NOT connected)
1. Open `tests/T02_buck` in VS Code (the folder with `platformio.ini`).
2. Plug in USB → **Build ✓** → **Upload →** → **Serial Monitor 🔌** → press **EN/RST**.
3. You should see:
   - RGB LED **white** for 2 s (normal power-on)
   - Blue LED blinking once per second
   - A short **green** RGB flash every ~4 s (Wi-Fi scan found networks)
   - Serial: `Reset reason: power-on (or EN button)`, then `Scan 1: … network(s)`, …
4. **Unplug the USB cable from the board.** It must stay unplugged for all of B2.

### B2. Power the ESP32 from the buck converter
**Before you power on:**
- [ ] **USB cable unplugged from the ESP32.** (Never USB and the 5V pin at
      the same time. See [SAFETY.md](../../docs/SAFETY.md) §2.)
- [ ] Supply ON for a moment: the meter still shows **4.95–5.05 V** at OUT+/OUT−.
      Then supply **OFF**.
- [ ] If your supply has a current limit, raise it to **0.5 A**. The ESP32 needs
      up to ~0.4 A at 5 V, which is ≈0.2 A at 12 V (see the study page).
- [ ] Buck **OUT+** → ESP32 **5V** pin (left row, bottom).
      Buck **OUT−** → ESP32 **GND** pin (any). Check: OUT+ is NOT on a 3.3V pin.
- [ ] Meter probes stay on OUT+ / OUT−.

**Steps:**
1. Supply **ON**.
2. Watch the LEDs and the meter for **at least 1 minute** (≈15 scans).

### You should see
| Check | Expected | If not |
|---|---|---|
| RGB at start-up | **White** for 2 s | **Red** = brown-out: voltage dipped. Recheck the setting and wires. |
| Blue LED | Blinks 1× per second, all the time | Stops or restarts → the supply dips. Tell me. |
| RGB after each scan | Short **green** flash | **Red** flash = no networks found (fine if you're far from Wi-Fi) |
| Meter during scans | 4.9–5.1 V, does not drop below 4.8 V | Tell me the lowest value you see |
| Supply current (12 V) | ≈0.1–0.2 A | Much higher → switch OFF |
| Buck module | Cold to slightly warm | Hot → switch OFF |

3. Supply **OFF**. Disconnect the buck from the ESP32 **before** plugging USB
   back in.

---

## Please report back
1. The final Part A voltage, and whether it stayed the same after OFF → ON (and at 24 V, if you tried).
2. Part B: the start-up colour, whether the blue LED blinked the whole minute,
   and the **lowest** voltage you saw on the meter.
3. The supply current in Part B (if your supply shows it).
4. Did you solder the wires or use clips?
