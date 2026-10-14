"""Test tool for T06 (NOT the Pi application).

Reads the ESP32's JSON Lines over USB, checks every line against protocol v1
(docs/PROTOCOL.md), counts lost lines (seq gaps) and shows the message rate.
Type a command (z, o 230, c, c 900, d, r, i, p, h) and press Enter to send it.

Usage:   python tools/serial_check.py COM5          (Windows)
         python3 tools/serial_check.py /dev/ttyUSB0 (Pi / Linux)
Needs:   pip install pyserial
Self-test without hardware:  python tools/serial_check.py --selftest
"""

import json
import sys
import threading
import time

REQUIRED = {
    "pos": {"count", "deg", "mm", "len_mm", "v_mms", "zeroed"},
    "hb": {"count", "deg", "mm", "len_mm", "v_mms", "zeroed"},
    "hello": {"fw", "proto", "mm_per_count", "mm_per_turn", "offset_mm", "dir", "zeroed"},
    "ack": {"cmd", "ok", "msg"},
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
        if t == "hello":
            return (f"HELLO fw {msg['fw']} proto {msg['proto']}  {msg['mm_per_turn']} mm/turn  "
                    f"offset {msg['offset_mm']} mm  zeroed {msg['zeroed']}")
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
    ]
    results = [c.feed(x) for x in lines]
    assert results[0] is None                       # boot text ignored
    assert results[1]["type"] == "hello"
    assert results[4]["len_mm"] == 40.0
    assert c.lost == 1                               # seq 4 missing
    assert results[5] is None                        # truncated line rejected
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

    threading.Thread(target=send_commands, daemon=True).start()
    ser.write(b"i\n")
    print("Listening. Type a command + Enter (z, o 230, c, c 900, d, r, i, p, h). Ctrl+C to stop.")
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
