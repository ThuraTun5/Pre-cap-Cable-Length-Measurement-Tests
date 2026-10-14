# T05 — Encoder → mm

**What this test is about:** the ESP32 now shows the slider travel in **mm**.
One encoder turn = **40 mm** of belt (20 teeth × 2 mm). You can zero it,
calibrate it against a known distance, and flip the direction. The settings stay
saved after power-off.

The belt isn't mounted yet, so for now **turn the encoder shaft by hand**:
1 turn = 40 mm. The real calibration on the belt comes later (section B).

**Wiring:** exactly as in T04. Nothing changes.

> Typing commands: click into the Serial Monitor, type the letters (you may not
> see them appear), then press **Enter**.

---

## A. Test now (by hand)

1. Open `tests\T05_mm` → **Build** → **Upload** → **Serial Monitor** → press **EN**.
2. Type `z` + Enter. That's the zero (later: slider pushed against its end stop).
3. Turn the shaft **one full turn** (watch the angle reach 360 deg).
   You should see about **travel 40.00 mm**.
4. Turn **10 full turns** → about **400 mm**. Turn back → it goes down to 0 again.
5. **Calibration practice:**
   - type `c` + Enter (start mark)
   - turn exactly **5 full turns** (the angle goes up by 1800 deg)
   - type `c 200` + Enter
   - it should say `Calibrated … mm per turn 40.0…` (almost no change, because 5 turns really are 200 mm)
6. **Direction:** if the number goes down when you turn the way the slider will move *out*, type `d` + Enter.
7. **Saved settings:** unplug USB, plug it back in. The settings printed at the start
   must be the same as before (calibration, direction).
8. Type `r` + Enter to put the calibration back to the nominal 40 mm per turn.

### You should see
```
travel     40.00 mm   length     40.00 mm   speed    38.5 mm/s   (count 8192, 360.0 deg)
```
| Check | Expected |
|---|---|
| 1 turn | ≈ 40.00 mm |
| 10 turns | ≈ 400.00 mm |
| Back to start | ≈ 0.00 mm |
| Calibration practice (5 turns, `c 200`) | change ≈ 0 % |
| After unplug / replug | same settings |

---

## B. Later, when the belt is mounted (real calibration)

1. Push the slider to its **end stop** → `z`.
2. Put a mark (ruler or reference part) at a **start** position → move the slider there → `c`.
3. Move the slider an **exactly known** distance. Longer is better, e.g. **900 mm** → `c 900`.
4. Check: move back to the start mark; travel should match within about 0.5 mm.
5. Measure the cable length at zero once → `o <that length>`. Now "length" = real cable length.

**Tip:** the longest known distance gives the best result. A ±0.5 mm reading
error over 900 mm is only ±0.06 %.

---

## Send me
- The travel after 1 turn and after 10 turns.
- The line printed after `c 200`.
- Whether the settings stayed after unplugging.
