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
| T2 | Buck converter set to 5.0 V + ESP32 as load | Signed off by user 2026-10-13 (no test results reported) |
| T3 | 74LVC245 level shifter (meter + ESP32 reads 3 channels) | Passed 2026-10-13: B1 = 3.3 V with 5 V in; ESP32 read HIGH/LOW on GPIO35 (one-channel check) |
| T4 | AMT102-V encoder → counts, degrees, index check (8192/turn) | Delivered (code compiled, study page ready) — waiting for your hardware test |
| T5 | Encoder → mm (GT2 20T, 40 mm/turn) + first calibration | Not started |
| T6 | Wi-Fi link + data format document | Not started |
| T7 | One TMC2209 + one motor (UART, current limit) | Not started |
| T8 | Second driver, synchronized motors | Not started |
| T9 | Motor logic: start / pause / reverse / reset (unwind) | Not started |
| T10 | Web page control | Not started |
| A | Assembly: full firmware, calibration procedure, docs | Not started |

## Reserved for later (room is left in pins + code)
- Emergency stop, connection watchdog, post-E-stop angle warning
- Touchscreen (final UI)
- Motor 3 (camera rail, different driver)

## Shopping list
- [ ] 100 nF (0.1 µF, "104") ceramic capacitors, through-hole, 50 V, a pack of ~100: needed before T4
  (e.g. <https://www.amazon.co.jp/dp/B01HNZPI8I>)
- [ ] Inline fuse holder + fuses (1–3 A range) — needed before T7 (or T2 if the
      supply has no current limit)
