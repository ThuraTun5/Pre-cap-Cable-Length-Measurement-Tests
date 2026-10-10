# T01 — ESP32 Board Bring-up (USB only)

**Goal:** prove that the board, USB cable, PC driver and PlatformIO toolchain all
work before we connect anything else.

**Risk level:** very low (USB power only, 5 V / max ~0.5 A from your PC).

**Wiring diagram:** [wiring.svg](wiring.svg) — just the USB-C cable.

---

## Before you power on — checklist
- [ ] **Nothing** is connected to any of the board's pins (no wires, no breadboard).
- [ ] The board sits on a **non-conductive surface** (table, paper, plastic bag) —
      not on metal, loose screws/wires, or black conductive foam.
- [ ] You use a **data** USB-C cable (some cheap cables are charge-only and will not
      show a COM port).
- [ ] You have read [docs/SAFETY.md](../../docs/SAFETY.md).

---

## Step 1 — Open the project
1. Open **VS Code**.
2. **File → Open Folder…** and choose
   `C:\Claude\Stepper Motors and Rotary Encoder\tests\T01_board`
   (open *this* folder, not the top-level folder — PlatformIO needs to see
   `platformio.ini` at the top).
3. Wait until PlatformIO finishes "Initializing" (bottom status bar). The first
   time may take a few minutes — it downloads the ESP32 tools.

## Step 2 — Connect the board
1. Plug the USB-C cable into the board, then into the PC.
2. Expected: the **ON** LED on the board lights up.
3. Open **Device Manager → Ports (COM & LPT)**. You should see something like
   `USB-SERIAL CH340 (COM5)`.
   - **No new COM port?** Try another cable first (most common cause). If that
     does not help, install the CH340 driver from Freenove:
     <https://github.com/Freenove/Freenove_ESP32_WROOM_Board/tree/main/CH340>

## Step 3 — Build
1. Click the **✓ (Build)** button in the PlatformIO toolbar at the bottom.
2. Expected: `SUCCESS` in green at the end of the terminal output.

## Step 4 — Upload
1. Click the **→ (Upload)** button.
2. Expected: `Writing at 0x000... (100 %)` and then `SUCCESS`.
   - **"Failed to connect to ESP32: Timed out waiting for packet header"**:
     click Upload again, and when you see `Connecting....` **hold the BOOT button**
     on the board for ~2 seconds, then release.

## Step 5 — Open the serial monitor
1. Click the **🔌 (Serial Monitor)** button.
2. Press the **EN/RST** button on the board once to restart the program.

### You should see
| What | Expected result |
|---|---|
| Blue LED "IO2" | Blinks on/off every 0.5 s |
| RGB LED "16" | Changes red → green → blue → off, once per second |
| Serial monitor | `=== T01 ESP32 board bring-up ===` followed by chip info |
| Chip info | `Chip model : ESP32-D0WD-V3`, `CPU cores : 2 @ 240 MHz`, `Flash size : 4 MB` (model/revision text may differ slightly) |
| Wi-Fi list | A list of nearby networks with signal strength (dBm) |

## Step 6 — Try the commands
Click into the serial monitor, type a letter and press **Enter**:

| Type | What happens |
|---|---|
| `h` | Shows the help line |
| `i` | Prints chip info again |
| `s` | Scans Wi-Fi again |
| `b` | Blue LED blinking ON/OFF |
| `r` | RGB colour cycling ON/OFF |

---

## Please report back
1. Did all 5 rows of "You should see" match? (Yes / which one didn't)
2. Copy-paste the **chip info** block and the **COM port name** into the chat.
3. Did you need the BOOT button or the CH340 driver?
4. Roughly how many networks were found, and what was the strongest dBm?

## Troubleshooting
| Problem | Fix |
|---|---|
| Serial monitor shows garbage characters | Make sure `monitor_speed = 115200` (it is set in `platformio.ini`) |
| Serial monitor is empty | Press EN/RST once; make sure the monitor is open on the right COM port |
| Upload says the port is busy | Close the serial monitor (or any other program using the COM port), then upload |
| Board gets warm | A little warmth near the module is normal. **Hot = unplug immediately** and tell me |
