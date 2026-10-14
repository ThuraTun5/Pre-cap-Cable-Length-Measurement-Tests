# ESP32 ↔ Raspberry Pi — Serial Protocol v1

For the teammate who writes the Raspberry Pi program. The ESP32 measures the
slider (encoder → mm). The Pi stores the data in SQL and shows it.

**Status:** v1, delivered in test T06 (2026-10-14). Motor fields will be *added*
in later tests; existing fields will not change meaning.

---

## 1. Physical link

| Item | Value |
|---|---|
| Cable | USB (ESP32 board USB-C ↔ Pi USB-A). The Pi also powers the ESP32 through it. |
| Device on the Pi | `/dev/ttyUSB0` (CH340 chip). Prefer the stable name `/dev/serial/by-id/usb-1a86_USB_Serial-if00-port0` (check with `ls /dev/serial/by-id/`). |
| Settings | **115200 baud, 8 data bits, no parity, 1 stop bit**, no flow control |
| Encoding | UTF-8 text, **one JSON object per line**, each line ends with `\n` ("JSON Lines") |
| Permission | the Pi user must be in the `dialout` group: `sudo usermod -a -G dialout $USER`, then log out and in |

### ⚠️ Opening the port can reset the ESP32
The board's auto-reset circuit uses the DTR/RTS lines. If the Pi program opens the
port with the default settings, the ESP32 may restart, and **the zero is lost**.
Set both lines off *before* opening:

```python
import serial
ser = serial.Serial()
ser.port = "/dev/ttyUSB0"
ser.baudrate = 115200
ser.timeout = 1
ser.dtr = False      # before open()!
ser.rts = False      # before open()!
ser.open()
```

Open the port **once** and keep it open. If the ESP32 restarts anyway, it sends a
`hello` message with `"zeroed": false`. Then the operator must push the slider to
the end stop and zero it again (command `z`).

### Lines to ignore
At power-on the ESP32 prints some boot text that isn't JSON (sometimes garbled).
**Ignore every line that doesn't start with `{`**, and ignore lines that fail to
parse as JSON.

---

## 2. ESP32 → Pi messages

Every message has these three fields:

| Field | Type | Meaning |
|---|---|---|
| `type` | string | `"pos"`, `"hb"`, `"hello"` or `"ack"` |
| `seq` | integer | message number, +1 for every line. A gap = a lost line. Restarts at 1 after an ESP32 restart. |
| `t_ms` | integer | ESP32 time since start-up in ms (wraps after 49.7 days). **Use the Pi's clock for the real timestamp** when you store a row. |

### `pos` — the slider moved
Sent when the position changed, **at most 25 times per second** (every 40 ms).
```json
{"type":"pos","seq":57,"t_ms":12840,"count":40960,"deg":1800.00,"mm":200.00,"len_mm":430.00,"v_mms":152.3,"zeroed":true}
```
| Field | Unit | Meaning |
|---|---|---|
| `count` | counts | raw encoder count since zero (8192 per encoder turn) |
| `deg` | degrees | encoder angle since zero (count × 360 / 8192); can exceed 360 |
| `mm` | mm | slider travel since zero (calibrated) |
| `len_mm` | mm | **cable length** = offset + travel. This is the main measurement. |
| `v_mms` | mm/s | slider speed (+ = away from the end stop) |
| `zeroed` | bool | `false` until the operator zeroed after start-up. **Don't trust `mm`/`len_mm` while false.** |

### `hb` — heartbeat
Same fields as `pos`, sent **once per second when nothing moves** (`v_mms` = 0).
If no line arrives for more than ~2 s, the link is broken.

### `hello` — start-up / info
Sent after every ESP32 start-up and on command `i`.
```json
{"type":"hello","seq":1,"t_ms":205,"fw":"T06","proto":1,"mm_per_count":0.0048828,"mm_per_turn":40.0000,"offset_mm":230.00,"dir":1,"zeroed":false}
```
| Field | Meaning |
|---|---|
| `fw` | firmware name/version |
| `proto` | protocol version (this document = 1) |
| `mm_per_count`, `mm_per_turn` | current calibration |
| `offset_mm` | cable length when the slider is at zero |
| `dir` | +1 normal, −1 flipped |

### `ack` — answer to a command
```json
{"type":"ack","seq":60,"t_ms":13502,"cmd":"z","ok":true,"msg":"zero set"}
```
`ok` = false means the command was refused; `msg` says why. Every command gets exactly one `ack`
(except `i` → `hello`, and `p` → `pos`).

---

## 3. Pi → ESP32 commands

One command per line, ending with `\n`. Plain text, not JSON.

| Command | Meaning | Reply |
|---|---|---|
| `z` | zero: slider is at the end stop → travel = 0 | ack |
| `o 230.0` | set the offset (cable length at zero) in mm, saved | ack |
| `c` | calibration step 1: slider is at the start mark | ack |
| `c 900` | calibration step 2: slider moved exactly 900 mm from the start mark, saved | ack (ok=false if not plausible) |
| `d` | flip direction, saved | ack |
| `r` | reset calibration to 40 mm per turn, saved | ack |
| `i` | send `hello` | hello |
| `p` | send one `pos` now | pos |
| `h` | list commands | ack |

**Reserved for later tests (motors):** upper-case letters (for example Start/Pause/
Reverse/Reset of the clamp-head motors) will be added in T9. Lower-case letters
above keep their meaning.

---

## 4. Minimal Pi reading loop (example)

```python
import json, serial, time

ser = serial.Serial()
ser.port, ser.baudrate, ser.timeout = "/dev/ttyUSB0", 115200, 1
ser.dtr = ser.rts = False
ser.open()

ser.write(b"i\n")                       # ask for hello
while True:
    raw = ser.readline().decode("utf-8", "replace").strip()
    if not raw.startswith("{"):
        continue                        # boot text or empty line
    try:
        msg = json.loads(raw)
    except json.JSONDecodeError:
        continue
    if msg["type"] in ("pos", "hb") and msg["zeroed"]:
        print(time.time(), msg["len_mm"])  # store this row in SQL here
```

---

## 5. Change log
| Version | Date | Change |
|---|---|---|
| 1 | 2026-10-14 | First version: pos, hb, hello, ack; commands z o c d r i p h |
