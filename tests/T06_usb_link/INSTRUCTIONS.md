# T06 — USB link to the Pi (data format)

**What this test is about:** the ESP32 now sends its measurements as clean data
lines (JSON) that the Pi program can read: length, travel, speed. You test it on
your laptop first; the Pi works exactly the same way.

**Wiring:** same as T04/T05 (encoder + chip). The USB cable goes to the laptop
(later: to the Pi).

**For your teammate:** [docs/PROTOCOL.md](../../docs/PROTOCOL.md) describes every
message and command.

---

## 1. See the data
1. Open `tests\T06_usb_link` → **Build** → **Upload** → **Serial Monitor** → press **EN**.
2. You should see lines like:
   ```
   {"type":"hello","seq":1,...,"zeroed":false}
   {"type":"hb","seq":2,...,"len_mm":0.00,...}        <- once per second
   ```
3. Type `z` + Enter → `{"type":"ack",...,"cmd":"z","ok":true,"msg":"zero set"}`
4. Turn the encoder → `{"type":"pos",...,"mm":40.00,...}` lines, about 25 per second.

## 2. Check it with the test tool
1. **Close the Serial Monitor** (trash-can icon on its terminal tab). Only one program can use the port.
2. In VS Code: **Terminal → New Terminal**, then run (use your COM number):
   ```
   & "$env:USERPROFILE\.platformio\penv\Scripts\python.exe" "C:\Claude\Stepper Motors and Rotary Encoder	ools\serial_check.py" COM5
   ```
3. Type `z` + Enter, turn the encoder. You should see:
   ```
   pos   seq    123  length     40.00 mm  travel     40.00 mm  speed    35.2 mm/s
   -- 412 messages, 24.8/s, lost lines: 0
   ```
4. **Reset check:** press **Ctrl+C**, then run the same command again.
   - Good: the first `HELLO` line says **zeroed True** (the ESP32 did not restart).
   - Bad: it says **zeroed False**, which means opening the port restarted the ESP32. Tell me.

## 3. (Later) on the Pi
Plug the ESP32 into the Pi, then on the Pi:
```
sudo usermod -a -G dialout $USER      (once; then log out and in)
sudo apt install python3-serial
python3 tools/serial_check.py /dev/ttyUSB0
```
Same results expected.

---

## Send me
- `lost lines: 0`? and the messages per second.
- The reset check result (zeroed True / False).
