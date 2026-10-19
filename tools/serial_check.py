"""Test tool for T06/T09 (NOT the Pi application).

Reads the ESP32's JSON Lines over USB, checks every line against the protocol
(docs/PROTOCOL.md, v1 encoder + v2 motors), counts lost lines (seq gaps) and
shows the message rate. Type a command and press Enter to send it:
  encoder (T06):  z, o 230, c, c 900, d, r, p
  motors  (T09):  S start, P stop, R reverse, X reset, M mirror, D drivers, Q status
  both:           i hello, h help
The tool also sends the "." keep-alive every 0.5 s, like the Pi app must do while
the motors turn (link watchdog). Use --no-keepalive to test the watchdog.

Usage:   python tools/serial_check.py COM5 [--no-keepalive]          (Windows)
         python3 tools/serial_check.py /dev/ttyUSB0 [--no-keepalive] (Pi / Linux)
Needs:   pyserial (PlatformIO's Python already has it; on the Pi: apt install python3-serial)
Self-test without hardware:  python tools/serial_check.py --selftest
"""

import json
import sys
import threading
import time

REQUIRED = {
    "pos": {"count", "deg", "mm", "len_mm", "v_mms", "zeroed"},
    "hb": {"count", "deg", "mm", "len_mm", "v_mms", "zeroed"},
    "hello": {"fw", "proto"},
    "ack": {"cmd", "ok", "msg"},
    "motor": {"state", "dir", "rpm", "turns", "clamp_deg", "steps", "diff_steps", "watchdog"},
    "fault": {"reason"},
}


def parse_line(raw):
    """Return (message dict, None) for a valid line, or (None, reason)."""
    raw = raw.strip()
    if not raw.startswith("{"):
        return None, "not JSON (ignored)"
    try:
        msg = json.loads(raw)
    except json.JSONDecodeError as exc:
        return None, f"bad JSON: {exc}"
    for key in ("type", "seq", "t_ms"):
        if key not in msg:
            return None, f"missing '{key}'"
    missing = REQUIRED.get(msg["type"], set()) - msg.keys()
    if msg["type"] not in REQUIRED:
        return None, f"unknown type '{msg['type']}'"
    if missing:
        return None, f"{msg['type']} missing {sorted(missing)}"
    return msg, None


class Checker:
    def __init__(self):
        self.last_seq = None
        self.lost = 0
        self.count = 0
        self.t0 = time.time()

    def feed(self, raw):
        msg, problem = parse_line(raw)
        if msg is None:
            if problem != "not JSON (ignored)":
                print(f"  !! {problem}: {raw.strip()[:80]}")
            return None
        if msg["type"] == "hello":
            self.last_seq = None          # ESP32 restarted: seq starts again
        if self.last_seq is not None and msg["seq"] != self.last_seq + 1:
            self.lost += msg["seq"] - self.last_seq - 1
        self.last_seq = msg["seq"]
        self.count += 1
        return msg

    def describe(self, msg):
        t = msg["type"]
        if t in ("pos", "hb"):
            flag = "" if msg["zeroed"] else "  (NOT ZEROED: send z)"
            return (f"{t:5} seq {msg['seq']:6}  length {msg['len_mm']:9.2f} mm  "
                    f"travel {msg['mm']:9.2f} mm  speed {msg['v_mms']:7.1f} mm/s{flag}")
        if t == "motor":
            return (f"MOTOR {msg['state']:9}  dir {msg['dir']:+d}  {msg['rpm']} rpm  turns {msg['turns']:8.3f}  "
                    f"clamp {msg['clamp_deg']:8.1f} deg  diff {msg['diff_steps']}")
        if t == "fault":
            return f"FAULT {msg['reason']}"
        if t == "hello":
            extra = {k: v for k, v in msg.items() if k not in ("type", "seq", "t_ms")}
            return f"HELLO {extra}"
        return f"ACK   '{msg['cmd']}' ok={msg['ok']}  {msg['msg']}"

    def summary(self):
        dt = max(time.time() - self.t0, 1e-9)
        return f"{self.count} messages, {self.count / dt:.1f}/s, lost lines: {self.lost}"


def selftest():
    c = Checker()
    lines = [
        "ets Jun  8 2016 00:22:57",
        '{"type":"hello","seq":1,"t_ms":205,"fw":"T06","proto":1,"mm_per_count":0.0048828,'
        '"mm_per_turn":40.0000,"offset_mm":0.00,"dir":1,"zeroed":false}',
        '{"type":"hb","seq":2,"t_ms":1205,"count":0,"deg":0.00,"mm":0.00,"len_mm":0.00,"v_mms":0.0,"zeroed":false}',
        '{"type":"ack","seq":3,"t_ms":1500,"cmd":"z","ok":true,"msg":"zero set"}',
        '{"type":"pos","seq":5,"t_ms":1540,"count":8192,"deg":360.00,"mm":40.00,"len_mm":40.00,"v_mms":12.5,"zeroed":true}',
        '{"type":"pos","seq":6,"t_ms":1580,"count":8192',
        '{"type":"hello","seq":1,"t_ms":200,"fw":"T09","proto":2,"rpm":10,"steps_per_turn":3200,'
        '"current_ma":700,"mirror2":true,"drivers_ok":true}',
        '{"type":"motor","seq":2,"t_ms":300,"state":"running","dir":1,"rpm":10,"turns":0.250,'
        '"clamp_deg":90.0,"steps":800,"diff_steps":0,"watchdog":true}',
        '{"type":"fault","seq":3,"t_ms":2400,"reason":"link watchdog: no line from the Pi for 2 s, motors stopped"}',
    ]
    results = [c.feed(x) for x in lines]
    assert results[0] is None                       # boot text ignored
    assert results[1]["type"] == "hello"
    assert results[4]["len_mm"] == 40.0
    assert c.lost == 1                               # seq 4 missing
    assert results[5] is None                        # truncated line rejected
    assert results[7]["state"] == "running" and results[7]["clamp_deg"] == 90.0
    assert results[8]["type"] == "fault"
    assert c.lost == 1                               # hello reset the seq check
    for r in results[6:]:
        print("  ", c.describe(r))
    print("selftest passed:", c.summary())


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return
    if sys.argv[1] == "--selftest":
        selftest()
        return
    import serial  # pyserial

    ser = serial.Serial()
    ser.port, ser.baudrate, ser.timeout = sys.argv[1], 115200, 0.5
    ser.dtr = False          # do not reset the ESP32 when opening
    ser.rts = False
    ser.open()
    checker = Checker()

    def send_commands():
        for line in sys.stdin:
            ser.write((line.strip() + "\n").encode())

    def keep_alive():
        while True:
            ser.write(b".\n")
            time.sleep(0.5)

    threading.Thread(target=send_commands, daemon=True).start()
    if "--no-keepalive" not in sys.argv:
        threading.Thread(target=keep_alive, daemon=True).start()
    ser.write(b"i\n")
    print("Listening. Type a command + Enter (S P R X M D Q / z o c d r p / i h). Ctrl+C to stop.")
    last_summary = time.time()
    try:
        while True:
            raw = ser.readline().decode("utf-8", "replace")
            if raw:
                msg = checker.feed(raw)
                if msg is not None:
                    print(checker.describe(msg))
            if time.time() - last_summary > 10:
                print("--", checker.summary())
                last_summary = time.time()
    except KeyboardInterrupt:
        print("\n--", checker.summary())
    finally:
        ser.close()


if __name__ == "__main__":
    main()
