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

## Wiring atlas (docs/wiring) — draft wiring for T2–T8
- Same Sky/CUI AMT10 datasheet: connector order B, 5V, A, X, G; 3.6–5.5 V; CMOS outputs, 2 mA;
  output high ≥ VDD − 0.8 V; DIP switches all OFF = 2048 PPR; stepper magnets can disturb the index:
  <https://cdn.automationdirect.com/static/manuals/surestepmanual/amt10datasheet.pdf>
- TI SN74LVC245A layout guidance (unused inputs must not float; OE pull-up advice):
  <https://www.ti.com/document-viewer/SN74LVC245A/datasheet/layout>
- TMC2209 pin functions (EN GND = on; MS1/MS2 = UART address bits, pulled down; VIO 3.3–5 V):
  <https://learn.watterott.com/silentstepstick/pinconfig/tmc2209/>
- TMC2209 single-wire UART with ESP32 (RX direct, TX via 1 kΩ; the module may already have a resistor):
  <https://forum.arduino.cc/t/tmc2209-and-esp32-uart-wiring/1362812>,
  <https://klipper.discourse.group/t/tmc2209-v1-3-confusion-and-more-confusion/16896>,
  <https://github.com/bigtreetech/BIGTREETECH-TMC2209-V1.2/issues/16>

## T2 — Buck converter
- ON Semiconductor LM2596 datasheet: Vout = Vref(1 + R2/R1), Vref = 1.23 V; 73 % at 12 V → 5 V, 3 A:
  <https://www.onsemi.com/download/data-sheet/pdf/Lm2596-d.pdf>
- Espressif ESP32-WROOM-32E datasheet v2.1, Tables 14/16: supply ≥ 0.5 A; Wi-Fi TX 239 mA avg / 379 mA peak:
  <https://www.espressif.com/sites/default/files/documentation/esp32-wroom-32e_esp32-wroom-32ue_datasheet_en.pdf>
- LM2596 module manuals (clockwise usually raises the voltage; always measure while adjusting):
  <https://manuals.plus/asin/B0CPHN7VG1>, <https://www.phippselectronics.com/support/lm2596s-dc-dc-step-down-adjustable-power-supply-module/>
- Project listing screenshots: module input 4.2–60 V, 400 kHz, input ≥ output + 1 V at high current
  (60 V / 400 kHz don't match a genuine LM2596 → chip unconfirmed; trust the meter).
- External power on ESP32 dev boards (5V pin OR 3.3V pin, never with USB): <https://microcontroller-course.web.cern.ch/Exercises/DevBoard/ESP32_WROOM_32E.en>

## T3 — Level shifter
- TI SN74LVC245A datasheet (SCAS218): pinout (N/PDIP), DIR high = A→B, inputs to 5.5 V, VIH = 2.0 V /
  VIL = 0.8 V at VCC 2.7–3.6 V, VOH ≥ VCC − 0.2 V (100 µA), tpd ≤ 6.3 ns at 3.3 V, unused inputs at VCC or GND,
  OE pull-up only needed to keep outputs off during power-up: <https://www.ti.com/lit/ds/symlink/sn74lvc245a.pdf>
- ESP32-WROOM-32E datasheet Table 15: VIH = 0.75 × VDD (2.475 V), VIL = 0.25 × VDD; abs. max VDD 3.6 V.
- ESP32 errata 3.11 (GPIO36/39 pulled low ~80 ns when ADC/Wi-Fi powers up) as discussed on esp32.com:
  <https://esp32.com/viewtopic.php?p=137753>, <https://esp32.com/viewtopic.php?p=84884>

## T4 — Encoder counting
- ESP32Encoder library 0.11.8 (PlatformIO registry `madhephaestus/ESP32Encoder`): builds with espressif32 6.13.0 /
  Arduino 2.0.17 (legacy IDF 4.4 PCNT driver); 16-bit counter extended to int64 on ±32767 limit interrupts;
  getCount() uses portENTER_CRITICAL_SAFE (ISR-safe); setFilter max 1023, default 250.
  <https://github.com/madhephaestus/ESP32Encoder>
- AMT10 datasheet: A leads B for CCW (viewed from front); resolutions are pre-quadrature (×4); DIP all OFF = 2048;
  index once per turn; stepper magnets can disturb the index.

## T5 — Encoder → mm
- GT2 belt 2 mm pitch, 20-tooth pulley → 40 mm per turn: project file `Belt and Pulley.png` (KeeYees GT2 kit).
- Settings stored with the Arduino-ESP32 `Preferences` library (ESP-IDF NVS), core 2.0.17.
- User decisions (2026-10-14): zero = push slider to end stop + `z`; reference = ruler, known part or caliper
  (all available); slider travel ≈ 1 m; encoder not yet mounted on the belt.

## T6 — USB serial link to the Pi
- pyserial: set `dtr`/`rts` before `open()` to avoid the ESP32 auto-reset; flow control off:
  <https://pyserial.readthedocs.io/en/latest/pyserial_api.html>, <https://esp32.com/viewtopic.php?p=120226>
- Raspberry Pi 5 USB power: 600 mA downstream by default (1.6 A with a 5 A supply):
  <https://pip-assets.raspberrypi.com/categories/685-app-notes-guides-whitepapers/documents/RP-009856-WP-1-USB%20Power%20delivery%20on%20Raspberry%20Pi%205.pdf>
- /dev/ttyUSB* belongs to the `dialout` group: <https://techoverflow.net/2022/06/10/how-to-fix-dev-ttyusb0-or-dev-ttyacm0-permission-error/>

## T7 — TMC2209 + one motor
- BIGTREETECH TMC2209 V1.3 user manual (pinout EN/MS1/MS2/RX/TX/CLK/STEP/DIR, VM/GND/A2/A1/B1/B2/VIO/GND;
  UART on the 4th pin by default, 5th pin only via pad R10; 12–28 V; cooling above 1.2 A; sense resistors R110):
  <https://3d.nice-cdn.com/upload/file/BIGTREETECH_TMC2209_V1.3_User_Manual.pdf>, <https://global.bttwiki.com/TMC2209.html>
- TMC2209 datasheet (CLK to GND = internal clock; pdn_disable must be set with UART; I_scale_analog default 1 = VREF):
  <https://www.analog.com/media/en/technical-documentation/data-sheets/tmc2209_datasheet_rev1.09.pdf>,
  <https://www.mouser.com/pdfDocs/TMC2209_Datasheet_V103.pdf>
- ESP32 + TMC2209 single-wire UART practice (RX direct, TX via 1 kΩ): <https://forum.arduino.cc/t/tmc2209-and-esp32-uart-wiring/1362812>
- FastAccelStepper 1.3.4 source (checked in this project): MCPWM/PCNT driver uses PCNT_UNIT_0..5 → use DRIVER_RMT;
  RMT driver uses channel = stepper index: <https://github.com/gin66/FastAccelStepper>
- Arduino-ESP32 2.0.17 `neopixelWrite()` calls `rmtInit()` (allocates an RMT channel): core source esp32-hal-rgb-led.c.

## Fuse (shopping)
- Inline 5×20 mm fuse holders + glass fuse assortment (0.1–20 A, incl. 1 A, 2 A, 3 A):
  <https://www.amazon.co.jp/dp/B098JVN9V1>
