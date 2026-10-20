# Development Plan & Status

Three phases: **Testing → Learning → Assembly.**
Every module follows the same cycle:

1. Research (datasheets, forums, library comparison) → 2. Plan check (pins, diagram,
safety) → 3. Build (one small PlatformIO project) → 4. Verify (compile, logic tests)
→ 5. Deliver (diagram, code, test steps, checklist) → 6. Learn (advlearn page)
→ 7. You test & sign off. **No module starts before the previous is signed off.**

## Status tracker
| Test | Component | Status |
|---|---|---|
| T0 | Setup: folders, git, PlatformIO, safety rules | Delivered |
| T1 | ESP32 board bring-up (USB only) | Signed off by user 2026-10-13 (no test results reported) |
| T2 | Buck converter set to 5.0 V + ESP32 as load | Signed off 2026-10-13. **No longer needed** since 2026-10-14: the Pi powers the ESP32 over USB |
| T3 | 74LVC245 level shifter (meter + ESP32 reads 3 channels) | Passed 2026-10-13: B1 = 3.3 V with 5 V in; ESP32 read HIGH/LOW on GPIO35 (one-channel check) |
| T4 | AMT102-V encoder → counts, degrees, index check (8192/turn) | Passed 2026-10-14: voltages OK (meter), counting + index check OK |
| T5 | Encoder → mm (GT2 20T, 40 mm/turn), zero (end stop + z), calibration, offset, direction (saved in NVS) | Part A passed 2026-10-14 (by hand: 40 mm/turn, calibration, saved settings OK). Part B (real calibration on the belt) moves to Assembly |
| T6 | **USB serial** link to the Pi + data format (docs/PROTOCOL.md v1) — Wi-Fi postponed by user | Signed off by user 2026-10-15 (no test results reported) |
| T7 | One TMC2209 + one motor (UART check, 700 mA software current, RMT step pulses) | Passed 2026-10-19 (user: everything OK) |
| T8 | Second driver (address 1), two synchronized motors, motor 2 mirrored | Passed 2026-10-19 (user: everything OK) |
| T9 | Motor commands from the Pi over USB: start / stop (hold) / reverse / reset (unwind every turn), motor state to the Pi, 2 s link watchdog | Passed 2026-10-20 (S/P/X smooth, test tool on COM7) |
| T10 | **Whole wiring together** (encoder + level shifter + both drivers on one breadboard, all running at once). Replaces the old web-page T10 (Wi-Fi postponed). Then: perfboard soldering plan | Delivered 2026-10-20 (`tests/T10_all_together`, atlas tabs T10 ①–③ with every pin), waiting for the hardware test |
| A | Assembly: full firmware, calibration procedure, docs | Not started |

## Reserved for later (room is left in pins + code)
- Emergency stop, connection watchdog, post-E-stop angle warning
- Touchscreen (final UI)
- Motor 3 (camera rail, different driver)

## Shopping list
- [x] 100 nF (0.1 µF, "104") ceramic capacitors, through-hole, 50 V, a pack of ~100: needed before T4
  (e.g. <https://www.amazon.co.jp/dp/B01HNZPI8I>)
- [ ] Perfboard for the soldered version: 9×15 cm double-sided FR4 (e.g. <https://www.amazon.co.jp/dp/B09L72LXC5>),
      female headers, a 20-pin DIP socket, 5.08 mm screw terminals — needed after T10
- [ ] Inline fuse holder + fuses (1–3 A range) — needed before T7 (or T2 if the
      supply has no current limit)
