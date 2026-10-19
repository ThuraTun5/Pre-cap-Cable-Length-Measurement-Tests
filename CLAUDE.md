# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

ESP32 firmware for a KUAS capstone project (assignment 2026PUZ1, sponsored by Pew's): a jig that inspects sensor cables about 1000 mm long.
- The cable is clamped between a fixed support and a sliding support that is **moved by hand**.
- Two steppers rotate the two clamp heads **in sync**, so a camera can see the cable from all sides.
- An AMT102-V encoder on the slider measures how far it moved, which gives the cable **length**. Target: ≤1 mm error per 1000 mm.
- The ESP32 sends the readings to a Raspberry Pi 5 over a USB cable (serial). Wi-Fi is postponed.

**Scope: ESP32 side only.** A teammate writes the Pi software (SQL storage and display). Never write Pi application code. Only document the data format the Pi will receive. Project background is in `Project Contents.pdf` and `Chasis/`. Part datasheets and photos are in the component folders at the repo root.

## Commands

PlatformIO is installed but is **not on PATH**. Use the full path (Git Bash):

```bash
PIO="$USERPROFILE/.platformio/penv/Scripts/pio.exe"
"$PIO" run -d tests/T01_board                      # build one test project
"$PIO" run -d tests/T01_board -t upload            # upload (needs the board on USB)
"$PIO" device monitor -d tests/T01_board           # serial monitor (115200)
"$PIO" check -d tests/T01_board --skip-packages    # cppcheck static analysis
```

Every `tests/Txx_*` folder is its own PlatformIO project, so pass that folder with `-d`. There are no automated unit tests yet. "Verify" means a clean build plus `pio check`. Real hardware tests are done by the user, who reports the results back.

## Architecture and conventions

- **Pinned platform:** `platform = platformio/espressif32 @ 6.13.0` (Arduino-ESP32 core **2.0.17**, ESP-IDF 4.4) and `board = esp32dev`. Use the 2.0.x APIs. For example, `neopixelWrite()` is correct; `rgbLedWrite()` is the 3.x name. The newer platform 7.x still ships core 2.0.17. Don't switch to pioarduino or core 3.x without asking.
- **Layout:**
  - `tests/Txx_name/`: one small, separate project per component test. Each holds `platformio.ini`, `src/main.cpp` and `INSTRUCTIONS.md`. Wiring diagrams live in one place: the interactive atlas `docs/wiring/index.html` (published as an Artifact), one tab per test. Draw real wires to pins or rail bars, never floating net-name tags (the user rejected that style).
  - `learn/Txx_name/index.html`: the study page for each test, also published as an Artifact.
  - `firmware/`: the final assembled program. Stays empty until the Assembly phase.
- **Docs are the source of truth.** Read them before changing hardware-related code:
  - `docs/PROJECT_PLAN.md`: test order T0–T10 plus Assembly, and the status tracker. Update it whenever you deliver something.
  - `docs/PIN_PLAN.md`: a **draft** pin map. Choose pins only from it, and confirm or change it inside the test that uses the pin.
  - `docs/SAFETY.md`: wiring and power rules. Every `INSTRUCTIONS.md` repeats the relevant ones as a "before you power on" checklist.
  - `docs/SOURCES.md`: research log. Add the datasheets and forum threads used for each test.
- **Code style:** `millis()`-based non-blocking `loop()`, never `delay()` in the loop. Pins are `constexpr` names, and serial input is one-letter commands. The final firmware depends on this pattern: encoder, motors, the USB link and the UI all run in one loop.

## Development process (set by the user — follow strictly)

1. Do one component test at a time, in the order in `docs/PROJECT_PLAN.md`. **Don't start the next test until the user signs off the current one.** Don't merge modules before the Assembly phase.
2. Cycle for each test: **research online** (official datasheets first, then forums; don't rely on memory) → check the plan (pins, diagram, safety) → build → verify (build + check) → deliver (code, wiring diagram, `INSTRUCTIONS.md` with "you should see" results and a checklist) → study page → the user tests.
3. Study pages use the user's `/anthropic-skills:advlearn` format, in **simple English** (the user is not a native speaker and is new to stepper motors). When there are design choices, explain the options and let the user pick.
4. The user decides when work begins. Wait for "start" before beginning a new phase.

## Hardware facts that drive decisions

- **Board:** Freenove ESP32-WROOM-32E, 40 pins, CH340 USB chip. On-board LEDs: GPIO 2 (blue) and **GPIO 16 (WS2812 RGB)**. GPIO 16 is kept as a status LED, so don't use it for UART2. Avoid the strapping pins 0/2/5/12/15. Pins 34/35/36/39 are input-only. **Don't use GPIO36/39 for encoder signals:** Espressif errata 3.11 says they get pulled low for about 80 ns when the ADC (used by Wi-Fi) powers up. Encoder A/B/X are on GPIO34/35/32, wired through 74LVC245 B2/B1/B3 (confirmed in T3).
- **USB + external 5 V on the 5V pin at the same time is forbidden.** The board schematic isn't public, so there may be no protection diode. The ESP32 runs on USB only during tests.
- **Encoder:** AMT102-V, 5 V output, on the shaft of a GT2 20-tooth pulley. One turn = **40 mm** of travel. Set to 2048 PPR ×4 = 8192 counts per turn, about 0.0049 mm per count. Read it with the hardware PCNT counter. Signals go through an SN74LVC245AN (DIP, VCC 3.3 V, 5 V-tolerant inputs). Cable length = fixed offset + slider travel, with a calibration step against a reference length. The zero/index method is still open: offer the user options.
- **Motors:** 2 pancake NEMA17 motors, 1 A/phase, on TMC2209 V1.3 drivers in **UART mode**, sharing one bus with different addresses. Limit the current to about 1 A (the module's default is 2 A). The motors face each other, so one direction is inverted in software. Motor supply 24 V (12–24 V) feeds only the drivers' VM. The LM2596S buck converter (tested in T2) is **no longer used**: the Pi powers the ESP32 over USB, and the encoder takes 5 V from the ESP32 5V pin.
- **Motor behaviour:** Start (runs continuously) / Pause / Reverse / Reset. The step counter starts at Start. **Reset unwinds every turn made**, not the shortest path. All interfaces send the same commands to one control core: the Pi app over the USB link during development (the ESP32 web page is postponed with Wi-Fi), a touchscreen on the ESP32 at the end.
- **Data to the Pi:** **USB serial, not Wi-Fi** (user decision 2026-10-14; Wi-Fi postponed). JSON Lines at 115200 baud, defined in `docs/PROTOCOL.md`. Protocol v2: encoder `pos` (≤25/s on change), `hb` (1/s when idle), `hello`, `ack`; motors `motor` (on change, 5/s moving, 1/s still) and `fault`. Commands: lower-case z/o/c/d/r/i/p/h (encoder), upper-case S start / P stop (hold) / R reverse / X reset (unwind) / M mirror / D drivers / Q status / W watchdog, and `.` keep-alive. 2 s link watchdog while the motors move. The Pi's web UI (on a display) is the only user interface; there are no buttons on the ESP32. Keep existing fields' meaning when extending the protocol, bump `proto`, and update the change log. The Pi must set DTR/RTS off before opening the port, or the ESP32 resets and loses its zero. The Pi powers the ESP32 over this USB cable, so the buck converter must never feed the ESP32 5V pin.
- **Deferred, but leave room** (pins are reserved in PIN_PLAN): hardware emergency stop to the drivers' EN pins, a connection-loss watchdog, a warning that the angle may be off after an E-stop, the touchscreen, and a 3rd motor for the camera rail (a different model and driver).

## Known gotchas

- The Arduino core defines `RGB_BRIGHTNESS` as a macro. Using that name for a constant fails with "expected unqualified-id before numeric constant". Other core macro names can do the same.
- The first build of a fresh environment downloads several hundred MB of toolchain.
- **PCNT/RMT resource clash (found in T7):** FastAccelStepper's default ESP32 step driver (MCPWM/PCNT) hard-codes PCNT units 0–5, and ESP32Encoder also takes PCNT units from 0. So always use `engine.stepperConnectToPin(pin, DRIVER_RMT)`. The RMT driver uses RMT channel = stepper index (0, 1), and Arduino's `neopixelWrite()` also allocates RMT channels, so programs with motors don't use the RGB LED on GPIO16 (use the blue LED on GPIO2).
- **TMC2209 setup (T7):** BTT V1.3 module, R_SENSE 0.11 Ω (resistors marked R110). UART on the 4th left pin (PDN); the 5th pin is open unless pad R10 is bridged. Set `pdn_disable(true)`, `I_scale_analog(false)` (current from software, not from the VREF screw), `mstep_reg_select(true)`, and `toff(>0)`. CLK goes to GND (internal clock). UART only answers while VM is powered. Pinned libraries: TMCStepper 0.7.3, FastAccelStepper 1.3.4, ESP32Encoder 0.11.8.
- Commits use the repo-local identity `ThuraTun5 <102456914+ThuraTun5@users.noreply.github.com>` (never the personal Gmail). End commit messages with the Claude co-author line.
- **GitHub:** `origin` = https://github.com/ThuraTun5/Pre-cap-Cable-Length-Measurement-Tests (public; push over HTTPS, port 22 is blocked here). `Project Contents.pdf` and `Chasis/` are sponsor/university material: git-ignored, never publish. The local branch `backup/before-publish` holds the old history (with those files and the Gmail address): never push it.
