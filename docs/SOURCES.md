# Sources (research log)

Every test lists what was checked and where. Official documents first,
community discussions second.

## T0 / T1 — Board and toolchain
- Freenove ESP32 WROOM board repository (pinout, CH340 driver, Espressif datasheets):
  <https://github.com/Freenove/Freenove_ESP32_WROOM_Board>
- Freenove board pinout PDF — LED on GPIO 2, WS2812 RGB on GPIO 16, strapping pins:
  <https://github.com/Freenove/Freenove_ESP32_WROOM_Board/blob/main/Datasheet/Freenove_ESP32_WROOM_Board_Pinout.pdf>
- PlatformIO community — Freenove board LEDs (GPIO 2 + TX/RX LEDs):
  <https://community.platformio.org/t/freenove-esp32-wroom-board/53722>
- PlatformIO espressif32 platform releases (7.x still ships Arduino core 2.0.17,
  so we pin the mature 6.13.0): <https://github.com/platformio/platform-espressif32/releases>
- Arduino core 3.x vs PlatformIO status (pioarduino fork; advice to always pin
  the platform version): <https://community.platformio.org/t/esp32-arduino-core-3-x-and-platformio-status/42008>
- USB + external 5 V at the same time — depends on a protection diode that varies
  by board; Freenove's board schematic is not public → rule "USB only" in SAFETY.md:
  <https://esp32.com/viewtopic.php?p=39467>,
  <https://esp32.com/viewtopic.php?p=135762>

## Level shifter (preview for T3)
- 74LVC245A: inputs are overvoltage tolerant to 5.5 V with VCC = 1.2–3.6 V, so it
  can translate 5 V → 3.3 V: <https://nexperia.com/group/74lvc245a-74lvch245a>
  (TI SN74LVC245A datasheet to be checked in T3.)

## Fuse (shopping)
- Inline 5×20 mm fuse holders + glass fuse assortment (0.1–20 A, incl. 1 A, 2 A, 3 A):
  <https://www.amazon.co.jp/dp/B098JVN9V1>
