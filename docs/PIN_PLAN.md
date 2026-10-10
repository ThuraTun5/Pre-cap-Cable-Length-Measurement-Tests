# ESP32 Pin Plan — DRAFT

> **Status: DRAFT.** Each pin is confirmed (or changed) during its own test,
> after researching that component. Final version is produced at Assembly.

Board: **Freenove ESP32-WROOM board** (ESP32-WROOM-32E, 40 pins).
Source: [Freenove_ESP32_WROOM_Board_Pinout.pdf](https://github.com/Freenove/Freenove_ESP32_WROOM_Board/blob/main/Datasheet/Freenove_ESP32_WROOM_Board_Pinout.pdf)

## Board facts that drive the plan
| Pin(s) | Fact | Consequence |
|---|---|---|
| GPIO 2 | On-board blue LED "IO2"; strapping pin | OK for blinking only |
| GPIO 16 | On-board **WS2812 RGB LED** | Keep it as a status light (not for UART) |
| GPIO 1 / 3 | USB serial TX/RX (LEDs "TX"/"RX") | Never use |
| GPIO 0, 2, 5, 12, 15 | Strapping pins (affect boot) | Avoid for external signals |
| GPIO 34, 35, 36 (VP), 39 (VN) | Input only, no internal pull-ups | Perfect for encoder signals (driven by 74LVC245) |
| GPIO 6–11 | Internal flash | Not exposed on this board — good |

## Draft assignment
| Function | GPIO | Test | Notes |
|---|---|---|---|
| On-board LED | 2 | T1 | Blink test |
| On-board RGB status LED | 16 | T1 | Status colours later |
| Encoder A | 34 | T4 | via 74LVC245 (5 V → 3.3 V) |
| Encoder B | 35 | T4 | via 74LVC245 |
| Encoder index (Z) | 39 (VN) | T4 | via 74LVC245 |
| TMC2209 UART TX | 17 | T7 | via 1 kΩ to PDN_UART (to be confirmed) |
| TMC2209 UART RX | 4 | T7 | to PDN_UART |
| Motor 1 STEP | 25 | T7 | |
| Motor 1 DIR | 26 | T7 | |
| Motor 2 STEP | 27 | T8 | |
| Motor 2 DIR | 14 | T8 | GPIO14 toggles at boot → DIR only (harmless while drivers disabled) |
| Drivers EN (shared) | 13 | T7 | needs pull-up so drivers stay OFF during boot (to be confirmed) |
| **Reserved:** Motor 3 STEP / DIR | 32 / 33 | later | different driver model |
| **Reserved:** E-stop input | 36 (VP) | later | external pull-up needed |
| **Reserved:** Touchscreen (SPI) | 18, 19, 23, 5, 21, 22 | later | VSPI pins kept free |
| Spare | 0, 12, 15 | — | strapping, use with care |
