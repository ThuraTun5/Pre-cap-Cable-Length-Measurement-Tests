# Cable Inspection Jig — ESP32 Firmware (KUAS Capstone 2026PUZ1)

ESP32 side of the sensor-cable inspection jig:
- Reads an **AMT102-V** encoder on a hand-moved slider (GT2 belt, 20T pulley)
  and reports **degrees + millimetres** to a Raspberry Pi 5 over a USB cable
  (serial, [protocol](docs/PROTOCOL.md)). The Pi also powers the ESP32 through it.
- Drives **two synchronized stepper motors** (TMC2209, UART) that rotate the
  cable clamp heads: start / pause / reverse / reset.

The Raspberry Pi software is written by a teammate. This repository only covers
the ESP32.

## Folder layout
| Folder | Contents |
|---|---|
| `docs/` | [Safety rules](docs/SAFETY.md), [plan & status](docs/PROJECT_PLAN.md), [pin plan](docs/PIN_PLAN.md), [wiring atlas](docs/wiring/index.html), [sources](docs/SOURCES.md) |
| `tests/` | One small PlatformIO project per component test (T01, T02, ...) |
| `learn/` | Study pages for each test |
| `firmware/` | The final assembled program (Assembly phase) |
| Component folders | Photos/specs of the parts (ESP32, Stepper Motor, Driver, Buck, Chasis, Belt) |

## Tools
- VS Code + PlatformIO IDE extension
- Platform pinned: `platformio/espressif32 @ 6.13.0` (Arduino-ESP32 core 2.0.17)

**Read [docs/SAFETY.md](docs/SAFETY.md) before wiring anything.**
