# T04 — Encoder → ESP32

**What this test is about:** turn the encoder by hand and see the ESP32 count it.
One full turn must always be **8192 counts = 360°**. If that holds, no counts are lost.

Wiring drawing: [Wiring Atlas](../../docs/wiring/index.html) → tab **T4 Encoder**.

---

## 1. Mount the encoder

The encoder needs a **5 mm shaft** to sit on. Use either:
- the pulley shaft in the jig, **or**
- the shaft of one stepper motor. The motor stays **unplugged**; you just turn its shaft by hand.

Use the 5 mm sleeve from the encoder kit. Leave the DIP switches on the encoder
as they came (**all OFF** = 2048 pulses per turn).

## 2. Wire it (USB unplugged)

The chip stays as it is (leg 20 → 3V3, leg 10 → GND, leg 1 → 3V3, leg 19 → GND,
legs 5–9 → GND). **Remove the test wires** from chip legs 2, 3 and 4.

The encoder's pins are labelled on its housing: **B · 5V · A · X · G**.

| Wire from | To |
|---|---|
| Encoder **5V** | ESP32 **5V** |
| Encoder **G** | ESP32 **GND** |
| Encoder **B** | chip leg **2** |
| Encoder **A** | chip leg **3** |
| Encoder **X** | chip leg **4** |
| chip leg **18** | ESP32 **35** |
| chip leg **17** | ESP32 **34** |
| chip leg **16** | ESP32 **32** |

**Check twice before USB:**
- Encoder **5V and G are not swapped**. Swapped power can kill the encoder.
- No encoder wire goes straight to the ESP32 (only 5V and G do).
- Capacitor: if you still have no 100 nF, put your **100 µF** across chip legs
  **20 and 19** (stripe leg → 19). Better than nothing. Please buy the 100 nF.

## 3. Run it
1. Open the folder `tests\T04_encoder` in VS Code.
2. Plug in USB → **Build ✓** → **Upload →** → **Serial Monitor 🔌** → press **EN**.
   (The first build downloads the encoder library. That's normal.)
3. Turn the shaft **slowly, one full turn**.

## 4. What you should see

```
count     2048   angle     90.00 deg   turns   0.250
count     4096   angle    180.00 deg   turns   0.500
INDEX #1 at count 5120 (first one: reference)
...
INDEX #2 at count 13312  -> 8192 counts since last index  OK (one full turn)
```

| Try this | Expected |
|---|---|
| Turn slowly | Count goes up (or down) smoothly, about 8192 per turn |
| Keep turning the same way | Every INDEX line says **8192 … OK** |
| Turn **fast** a few turns | Still **8192 … OK** at every index |
| Turn back the other way | Count goes the other way; INDEX shows **−8192 … OK** |
| Type `z` + Enter | Count becomes 0 |
| RGB LED | Short **green** flash once per turn (index) |

**Not OK?**
- **Count doesn't change at all:** check encoder 5V/G, and chip legs 18 → 35 and 17 → 34.
- **Count only jumps back and forth by 1:** A or B is missing. Check encoder A → leg 3 and B → leg 2.
- **No INDEX lines:** check encoder X → leg 4 and leg 16 → 32. On a stepper motor
  shaft, the motor's magnet can disturb the index (the encoder datasheet warns
  about this). If A/B counting works, that's OK for now.
- **INDEX says CHECK:** counts were lost. Tell me the numbers.

## 5. Send me
- One INDEX line after a slow turn and one after fast turns.
- Which way you turned when the count went **up** (clockwise or counter-clockwise, looking at the shaft end).
