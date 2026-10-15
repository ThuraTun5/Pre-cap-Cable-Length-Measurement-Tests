# ESP32 Pin Plan — DRAFT

> **Status: DRAFT.** Each pin is confirmed (or changed) during its own test,
> after researching that component. Final version is produced at Assembly.

Board: **Freenove ESP32-WROOM board** (ESP32-WROOM-32E, 40 pins).
Source: [Freenove_ESP32_WROOM_Board_Pinout.pdf](https://github.com/Freenove/Freenove_ESP32_WROOM_Board/blob/main/Datasheet/Freenove_ESP32_WROOM_Board_Pinout.pdf)

## Board facts that drive the plan
| Pin(s) | Fact | Consequence |
|---|---|---|
| GPIO 2 | On-board blue LED "IO2"; strapping pin | OK for blinking only |
| GPIO 16 | On-board **WS2812 RGB LED** | Status light only in programs **without** motors: `neopixelWrite()` takes an RMT channel, and the motor step pulses use RMT channels 0/1 (T7). Motor programs use the blue LED (GPIO 2) |
| GPIO 1 / 3 | USB serial TX/RX (LEDs "TX"/"RX") | Never use |
| GPIO 0, 2, 5, 12, 15 | Strapping pins (affect boot) | Avoid for external signals |
| GPIO 34, 35, 36 (VP), 39 (VN) | Input only, no internal pull-ups | Good for signals driven by the 74LVC245 |
| GPIO 36, 39 | **Espressif errata 3.11:** pulled low for ~80 ns when the ADC (used by Wi-Fi) powers up | Not for encoder signals. Any input there needs a software filter |
| GPIO 6–11 | Internal flash | Not exposed on this board — good |

## Draft assignment
| Function | GPIO | Test | Notes |
|---|---|---|---|
| On-board LED | 2 | T1 | Blink test |
| On-board RGB status LED | 16 | T1 | Status colours later |
| Encoder A | 34 | T3 ✔ / T4 ✔ | via 74LVC245 B2 (5 V → 3.3 V) |
| Encoder B | 35 | T3 ✔ / T4 ✔ | via 74LVC245 B1 |
| Encoder index (X) | **32** | T3 ✔ / T4 ✔ | via 74LVC245 B3. Moved from 39 because of the errata (T3) |
| TMC2209 UART TX | 17 | T7 | via 1 kΩ to PDN (4th pin) |
| TMC2209 UART RX | 4 | T7 | direct to PDN (4th pin; the 5th pin is not connected inside, BTT V1.3 manual) |
| Motor 1 STEP | 25 | T7 | |
| Motor 1 DIR | 26 | T7 | |
| Motor 2 STEP | 27 | T8 | RMT channel 1 |
| Motor 2 DIR | 14 | T8 | GPIO14 toggles at boot → DIR only (harmless while drivers disabled); inverted in software (mirror) |
| Drivers EN (shared) | 13 | T7 | 10 kΩ pull-up to 3V3 keeps the drivers OFF during boot |
| **Reserved:** Motor 3 STEP / DIR | 33 / TBD | later | different driver model; second pin chosen when M3 is known |
| **Reserved:** E-stop input | 36 (VP) | later | external pull-up needed; filter in software (errata 3.11) |
| **Reserved:** Touchscreen (SPI) | 18, 19, 23, 5, 21, 22 | later | VSPI pins kept free |
| Spare | 0, 12, 15 | — | strapping, use with care |

## Draft wiring details (see the interactive [wiring atlas](wiring/index.html))
| Item | Draft connection |
|---|---|
| 74LVC245 channels | Encoder B → A1 (pin 2) → B1 (pin 18) → GPIO35 · Encoder A → A2 (pin 3) → B2 (pin 17) → GPIO34 · Encoder X → A3 (pin 4) → B3 (pin 16) → GPIO32. Order chosen so no wires cross. Confirmed in T3. |
| 74LVC245 control (T3 ✔) | VCC (20) and DIR (1) → 3V3 · GND (10) and OE (19) → GND · unused A4–A8 (5–9) → GND · 100 nF across pins 20/19 |
| Encoder power | ESP32 5V pin (USB powered, encoder draws about 6 mA) |
| TMC2209 UART | GPIO4 (RX) directly to PDN; GPIO17 (TX) through R1 1 kΩ to the same PDN node (check the module's two PDN pins in T7) |
| TMC2209 addresses | Driver 1: MS1 = GND, MS2 = GND → 0 · Driver 2: MS1 = 3V3, MS2 = GND → 1 |
| EN | GPIO13 → EN of both drivers, plus R2 10 kΩ pull-up to 3V3 (drivers OFF during boot) |
| Motor coils | A1 black (A+), A2 green (A−), B1 red (B+), B2 blue (B−). Check the pairs with a meter first |
