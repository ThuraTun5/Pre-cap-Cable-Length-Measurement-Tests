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

## Wire management study page (2026-10-22)
- Adafruit Guide to Excellent Soldering (iron ≥ 25 W, keep the joint still, fumes/lead, students ask their lab):
  <https://learn.adafruit.com/adafruit-guide-excellent-soldering>
- Tip temperatures (third-party, sources differ): 63/37 ≈ 320–350 °C, SAC305 ≈ 350–380 °C:
  <https://www.lcsc.com/blog/solder-melting-point-guide/>
- Same Sky AMT10 datasheet (CMOS 2 mA, VOH ≥ VDD − 0.8 V, mating plug Molex 50-57-9405):
  <https://www.sameskydevices.com/product/resource/amt10.pdf>
- Tinned ends cold-flow in screw clamps (EN 60204-1); use ferrules: Altech ferrule datasheet
  <https://datasheets.globalspec.com/ds/Altech/2776-0/b68971fe-f141-4ad8-8778-38d94fda948a>,
  RepRap <https://reprap.org/wiki/Wire_termination_for_screw_terminals>
- Solder wicking → flex point at the wicking edge: US 4385341 <https://patents.google.com/patent/US4385341>
- Strain relief (loop through holes, ties, no glue on pads):
  <https://www.eevblog.com/forum/manufacture/what-is-acceptable-ways-of-soldering-wires-directly-to-pcb/msg2496603/>,
  <https://forum.arcadecontrols.com/index.php/topic,3534.0.html>
- Perfboard technique: <https://en.wikipedia.org/wiki/Perfboard>,
  <https://www.digikey.com/en/maker/blogs/2022/start-building-cleaner-perfboard-projects-using-these-simple-tips>
- Stranded vs solid fatigue: <https://blog.epectec.com/what-is-the-difference-between-a-stranded-cable-and-solid-cable>
- NASA workmanship, cable bend radius (multi-core: 6× min, 10× long-term) and tie rules:
  <https://workmanship.nasa.gov/lib/insp/2%20books/links/sections/401%20General%20Requirements.html>
- igus drag chain length LK = S/2 (+ ΔM) + K, K = πR + 2T (T = pitch), fixed end in the middle:
  <https://www.igus.com/info/e-chain-designing-calculation>, <https://www.igus.com/info/e-chain-designing-terms>,
  bend factor <https://blog.igus.co.uk/?p=12439>
- Analog Devices / Trinamic, long motor cables (keep coil pairs apart; shielded twisted pairs or GND between pairs):
  <https://ez.analog.com/other-products/a/documents/do23379/how-can-i-fix-reduced-trinamic-motor-performance-and-increased-motor-noise-and-vibration-when-using-long-cables>
- Separate motor and encoder cables, shield drain at one end, one signal per pair:
  <https://forum.linuxcnc.org/49-basic-configuration/45218-encoder-wiring>,
  <https://buildbotics.com/archive/cad_cam_edm_dro/messages/49800.html>,
  <https://www.plctalk.net/threads/stepper-motor-cabling.119448/>
- Computed (not looked up): AWG diameter/resistance from the AWG formula and ρCu = 1.724e-8 Ω·m; 24 V supply
  current ≈ 0.34 A (4 coils × 0.7² A² × 4.1 Ω / 24 V); PCNT filter 100 ticks / 80 MHz = 1.25 µs.

## T11 — Number display (TM1637, 6 digits) (2026-10-23)
- Titan Micro TM1637 datasheet V2.4: VDD "typical 5 V", electrical tables only for VDD 4.5–5.5 V, absolute max 7 V,
  VIH = 0.7 VDD (CLK, DIO), clock < 250 kHz, typical circuit with 10 kΩ pull-ups + 100 pF on CLK/DIO,
  note: blue LEDs (~3 V drop) need a 5 V supply: <https://akizukidenshi.com/goodsaffix/TM1637_V2.4_EN.PDF>
  → At 3.3 V the chip runs outside the datasheet's tested range; community reports it works with red LEDs (slightly
  dimmer). At 5 V the ESP32's 3.3 V signals are below VIH (3.5 V) and the module pull-ups would feed 5 V into the
  ESP32. Decision: power the module from 3V3.
- ESP32 + TM1637 powered from 3.3 V: <https://lastminuteengineers.com/esp32-tm1637-tutorial/>,
  <https://www.letscontrolit.com/forum/viewtopic.php?p=63945>
- Pull-ups on CLK/DIO needed (TeWu library tested with 10 kΩ at 3.3 V): <https://github.com/TeWu/TM1637>,
  Espressif component readme <https://components.espressif.com/components/heronet/tm1637/versions/1.0.2/readme>
- 6-digit modules can show digits out of order ([2][1][0][5][4][3]); TM1637TinyDisplay6 remaps them:
  <https://forums.parallax.com/discussion/comment/1570642>, <https://forum.arduino.cc/t/tm1637-6-digit-led-display/702875>
- Chosen module (red, 0.56", decimal points, loose 4-pin header; back side shows 4 small parts next to the chip,
  most likely the datasheet's 2 pull-ups + 2 capacitors): <https://www.amazon.co.jp/dp/B0BPSP9557>

## Perfboard layout best practice (2026-10-23)
- Digi-Key: parts first, short straight insulated wire on top with 90° bends, component legs as links, sockets for parts
  you may replace: <https://www.digikey.com/en/maker/blogs/2022/start-building-cleaner-perfboard-projects-using-these-simple-tips>
- Wikipedia perfboard: bare wire only for buses with a clear lane, insulated solid wire elsewhere: <https://en.wikipedia.org/wiki/Perfboard>
- Arduino forum, breadboard → perfboard (keep wires horizontal/vertical, bottom links short):
  <https://forum.arduino.cc/t/from-breadboard-to-perfboard/108643>
- Don't hide wires under a socketed module (probing/debugging): <https://industrialmonitordirect.com/no/blogs/knowledgebase/esp32-module-wiring-perfboard-vs-wire-wrap-vs-custom-pcb-methods>,
  <https://learn.sparkfun.com/tutorials/esp32-thing-hookup-guide/assembly-tips>
- Watterott SilentStepStick: 100 µF low-ESR at every driver, VIO before STEP/DIR/EN: <https://learn.watterott.com/silentstepstick/pinconfig/>
- Bare bus wire next to pad rows = short-circuit risk: <https://forum.allaboutcircuits.com/threads/how-much-current-can-a-tin-solder-track-handle.171525/post-1534393>
→ Layout v2 (docs/perfboard): ESP32 moved down, 4-row open wiring channel, buses in a clear lane right below the drivers,
  unavoidable crossings become two-hole hops to the back side (plated through-holes).

## Encoder board soldering test (2026-10-28)
- Official Freenove ESP32 WROOM pinout (pin order of both rows; rows are 10 holes = 25.4 mm apart, measured on the image):
  <https://github.com/Freenove/Freenove_ESP32_WROOM_Board/blob/main/Freenove_ESP32_WROOM_Board_Pinout.png>.
  Left row (antenna end first): 3V3, EN, VP, VN, 34, 35, 32, 33, 25, 26, 27, 14, 12, GND, 13, 3.3V, 3.3V, 3.3V, 5V, 5V.
  Right row: GND, 23, 22, TX, RX, 21, GND, 19, 18, 5, 17, 16, 4, 0, 2, 15, GND, GND, GND, GND.
  → fixed the concept page (docs/perfboard): GND and GPIO13 labels were swapped.
- 9×15 cm prototype boards are 34 × 52 holes (Elecrow, Bits&Parts), not 35 × 59 as drawn on the concept page:
  <https://www.bitsandparts.nl/en/prototyping-board-pcb-fiberglass-(34x52-holes-9x15cm)-p118241>. Fix in the real soldering plan.
- Encoder test board (docs/soldering/encoder_board.html): same nets as the T4/T10 breadboard (B → A1 → B1 → GPIO35,
  A → A2 → B2 → GPIO34, X → A3 → B3 → GPIO32; DIR + VCC = 3.3 V; OE + 5–10 = GND; 100 nF at pins 19/20).
