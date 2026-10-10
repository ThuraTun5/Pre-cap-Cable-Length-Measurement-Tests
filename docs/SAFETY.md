# Safety Rules (read before every test)

These rules protect **you, your team, and the parts**. Every test's instructions
include a short "before you power on" checklist that builds on this page.

## 1. General wiring rules
1. **Power OFF while wiring.** Unplug the USB cable *and* switch off the bench
   supply before adding, removing or moving any wire.
2. **Check twice, power once.** Compare every wire with the test's diagram before
   switching on. Ask a teammate to check too.
3. **One common ground.** All GND pins (ESP32, buck converter, level shifter,
   encoder, drivers, power supply) must be connected together.
4. **Measure before you connect.** Use the multimeter to check a voltage *before*
   plugging a module into it.
5. **Polarity!** Red = +, black = −. Reversed polarity can destroy modules
   instantly. Check with the multimeter if unsure.
6. **Work on a non-conductive surface.** Never place boards on metal, on loose
   wires/screws, or on black conductive foam (it shorts the pins).
7. **If something smells hot, smokes, or a part gets too hot to touch:**
   switch the power OFF immediately and tell the team before trying again.

## 2. ESP32 board rules (Freenove ESP32-WROOM)
1. **Never put more than 3.3 V on any GPIO pin.** 5 V signals (like the encoder)
   must go through the 74LVC245 level shifter first.
2. **Do NOT power the board from USB and the 5V pin at the same time** until we
   have verified the board's power circuit. Freenove does not publish the board
   schematic, so we cannot confirm a protection diode exists. Back-feeding can
   damage your PC's USB port or the board.
   - During tests: power the ESP32 from **USB only**.
   - Other modules (encoder, level shifter) may be powered from the buck
     converter as long as **grounds are shared** and the buck's 5 V is **not**
     connected to the ESP32's 5V pin.
3. Avoid using strapping pins (GPIO 0, 2, 5, 12, 15) for external signals unless
   a test says so. They change how the ESP32 boots.
4. GPIO 34, 35, 36 (VP), 39 (VN) are **input only**.

## 3. Power supply and buck converter rules
1. **Set the buck converter output (≈5.0 V) with the multimeter BEFORE
   connecting anything to its output.**
2. Re-measure the output every time the input supply voltage changes
   (12 V ↔ 24 V).
3. Use a **fuse** on the main supply's + wire (rating chosen in test T7).
   If your supply has a current limit knob, set it low (≈0.3 A) for the
   low-power tests T2–T6.
4. The buck converter's input must be at least ~1 V higher than its output.

## 4. Stepper motor and TMC2209 driver rules (tests T7 onward)
1. **NEVER connect or disconnect a motor while the driver is powered.** The
   voltage spike destroys the TMC2209.
2. Add an electrolytic capacitor (≈100 µF, ≥35 V) across each driver's VM and GND,
   close to the driver, with correct polarity (stripe = −).
3. Set the motor current to **≈1 A or less** (motor rating: 1 A/phase).
   The driver's "2 A default" will overheat these motors.
4. Keep the heatsinks on. Don't touch drivers or motors while running — they get hot.
5. Clamp heads rotate: keep fingers, hair, sleeves and loose cables away.
   Start every new motor test at **low speed**.

## 5. Not implemented yet (reserved for later)
- Physical emergency-stop button (hardware cut to driver EN pins)
- Connection-loss watchdog
- Angle-uncertain warning after an E-stop

Until these exist, **the power switch of the motor supply is your emergency stop.**
Keep it within reach during every motor test.
