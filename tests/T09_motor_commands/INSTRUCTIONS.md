# T09 — Motor commands from the Pi

**What this test is about:** the Pi (or your laptop) controls both clamp-head
motors with short commands over USB: **S** start, **P** stop, **R** reverse,
**X** reset. The ESP32 reports what the motors do. **Reset** turns back every turn
since Start, so the clamp heads end at their start angle.

**Wiring:** exactly as in **T08** (both drivers, both motors). Nothing changes.
Same 3 rules: never plug or unplug a motor while motor power is on; USB first,
motor power last (off: motor power first); current limit about 1 A.

**For your teammate:** [docs/PROTOCOL.md](../../docs/PROTOCOL.md) (now version 2) describes the commands
and the `motor` / `fault` messages.

---

## 1. Upload
Open `tests\T09_motor_commands` → **Build** → **Upload**. Close the Serial Monitor
afterwards: we use the test tool instead (it sends the keep-alive the motors need).

## 2. Start the test tool (VS Code terminal, use your COM number)
```
& "$env:USERPROFILE\.platformio\penv\Scripts\python.exe" "C:\Claude\Stepper Motors and Rotary Encoder	ools\serial_check.py" COM5
```
Then **motor power ON**. Within 3 s you should see `MOTOR idle ...` lines
(once per second). If it stays `off`, type `D` + Enter.

## 3. Try the commands (type the letter, then Enter)
| Type | You should see |
|---|---|
| `S` | `ACK 'S' ok=True running`, motors turn **together** at 10 rpm, `turns` goes up |
| `P` | motors slow down and stop: `stopping` → `paused`, shafts **hold** |
| `S` | they continue; `turns` keeps counting from where it was |
| `R` | they slow down, turn round, go the other way |
| `P`, then `X` | `resetting`, they turn back **all** the turns → `idle`, `turns 0.000` |
| | the tape marks are back at their start positions |

`diff` must always stay **0** (both motors in sync).

## 4. Watchdog test
1. Ctrl+C to stop the tool. Start it again with `--no-keepalive` at the end of the command.
2. `S` → the motors start, and **after 2 s they stop by themselves**:
   `FAULT link watchdog: no line from the Pi for 2 s, motors stopped`.
3. `X` to reset (then, without keep-alive, it may stop again after 2 s; just send `X` again).
4. Ctrl+C, start the tool normally again.

## 5. Finish
`X` (back to start) → motor power OFF → USB.

## Send me
- Did S / P / S / R / X behave as in the table?
- Did Reset end exactly at the start marks?
- Did the watchdog stop the motors after 2 s?
