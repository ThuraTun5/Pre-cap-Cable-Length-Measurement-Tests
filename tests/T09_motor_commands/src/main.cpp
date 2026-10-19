// =============================================================================
// T09 - Motor commands from the Pi over USB (protocol v2, see docs/PROTOCOL.md)
// -----------------------------------------------------------------------------
// The Pi's web UI controls the two clamp-head motors with one-letter commands:
//   S start   P stop (hold)   R reverse   X reset (unwind every turn since start)
//   M mirror motor 2 (only at start position)   D re-check drivers   Q motor status now
//   W 0 / W 1  link watchdog off / on   .  keep-alive (no answer)   i hello   h help
// The ESP32 answers each command with an "ack" line and reports the motor state
// with "motor" lines (on every change, 5x per second while moving, 1x per
// second while still).
//
// Rules:
//   * The turn counter starts at 0 on Start from the start position. Stop keeps
//     it, so Start continues. Reset turns back every turn to 0 (start angle).
//   * Turning speed is a constant (TURN_RPM below).
//   * Link watchdog: while the motors move, the Pi must send a line at least every
//     2 s (the "." keep-alive is enough). Otherwise the motors stop and hold.
//   * Motors stay OFF until both drivers answer over UART (checked every 3 s).
//
// Wiring: same as T08 (two drivers on one UART bus, see the atlas tab T8).
// =============================================================================

#include <Arduino.h>
#include <TMCStepper.h>
#include <FastAccelStepper.h>
#include <Preferences.h>

// ---- Pins (PIN_PLAN, confirmed in T7/T8) ------------------------------------
constexpr uint8_t PIN_EN       = 13;
constexpr uint8_t PIN_STEP1    = 25;
constexpr uint8_t PIN_DIR1     = 26;
constexpr uint8_t PIN_STEP2    = 27;
constexpr uint8_t PIN_DIR2     = 14;
constexpr uint8_t PIN_UART_RX  = 4;
constexpr uint8_t PIN_UART_TX  = 17;
constexpr uint8_t PIN_LED_BLUE = 2;    // on = motors powered (no RGB LED: RMT is used for steps)

// ---- Protocol ---------------------------------------------------------------
constexpr int      PROTOCOL_VERSION   = 2;
constexpr uint32_t BAUD               = 115200;
constexpr size_t   TX_BUFFER_BYTES    = 2048;
constexpr uint32_t MOTOR_MOVING_MS    = 200;    // motor line while moving: 5x per second
constexpr uint32_t MOTOR_STILL_MS     = 1000;   // motor line while still: 1x per second
constexpr uint32_t WATCHDOG_MS        = 2000;   // no line from the Pi for 2 s -> stop

// ---- Drivers / motion -------------------------------------------------------
constexpr float    R_SENSE          = 0.11f;
constexpr uint32_t UART_BAUD        = 115200;
constexpr uint8_t  EXPECTED_VERSION = 0x21;
constexpr uint16_t MICROSTEPS       = 16;
constexpr uint32_t STEPS_PER_TURN   = 200UL * MICROSTEPS;     // 3200
constexpr uint16_t MOTOR_CURRENT_MA = 700;
constexpr uint16_t TURN_RPM         = 10;                     // constant turning speed (to be discussed)
constexpr uint32_t STEP_HZ          = (uint32_t)TURN_RPM * STEPS_PER_TURN / 60;
constexpr uint32_t ACCEL_STEPS_S2   = 3200;
constexpr uint32_t DRIVER_RETRY_MS  = 3000;
constexpr uint32_t HEALTH_MS        = 500;
constexpr uint8_t  NUM_MOTORS       = 2;

enum class State { Off, Idle, Running, Stopping, Paused, Resetting, Fault };

const char* stateName(State s) {
  switch (s) {
    case State::Off:       return "off";        // drivers not answering (motor power off?)
    case State::Idle:      return "idle";       // at the start position, holding
    case State::Running:   return "running";
    case State::Stopping:  return "stopping";   // slowing down after P (or watchdog)
    case State::Paused:    return "paused";     // stopped away from start, holding
    case State::Resetting: return "resetting";  // turning back to the start position
    case State::Fault:     return "fault";      // over-temperature / short: motors off
  }
  return "?";
}

TMC2209Stepper drivers[NUM_MOTORS] = {
  TMC2209Stepper(&Serial2, R_SENSE, 0b00),
  TMC2209Stepper(&Serial2, R_SENSE, 0b01),
};
FastAccelStepperEngine engine;
FastAccelStepper* steppers[NUM_MOTORS] = { nullptr, nullptr };
Preferences prefs;

State    state         = State::Off;
int8_t   runDir        = 1;        // +1 forward, -1 backward
bool     mirror2       = true;
bool     watchdogOn    = true;
uint32_t seq           = 0;
uint32_t lastRxMs      = 0;
uint32_t lastMotorMsgMs = 0;
uint32_t lastDriverTryMs = 0;
uint32_t lastHealthMs  = 0;
char     lineBuf[32];
uint8_t  lineLen       = 0;

// ---- Output -----------------------------------------------------------------
void sendLine(const char* type, const char* body) {
  seq++;
  Serial.printf("{\"type\":\"%s\",\"seq\":%lu,\"t_ms\":%lu%s%s}\n", type, (unsigned long)seq,
                (unsigned long)millis(), body[0] ? "," : "", body);
}

void sendAck(char cmd, bool ok, const char* msg) {
  char body[160];
  snprintf(body, sizeof(body), "\"cmd\":\"%c\",\"ok\":%s,\"msg\":\"%s\"", cmd, ok ? "true" : "false", msg);
  sendLine("ack", body);
}

void sendFault(const char* reason) {
  char body[160];
  snprintf(body, sizeof(body), "\"reason\":\"%s\"", reason);
  sendLine("fault", body);
}

void sendMotor() {
  int32_t p1 = steppers[0] ? steppers[0]->getCurrentPosition() : 0;
  int32_t p2 = steppers[1] ? steppers[1]->getCurrentPosition() : 0;
  char body[220];
  snprintf(body, sizeof(body),
           "\"state\":\"%s\",\"dir\":%d,\"rpm\":%u,\"turns\":%.3f,\"clamp_deg\":%.1f,\"steps\":%ld,"
           "\"diff_steps\":%ld,\"watchdog\":%s",
           stateName(state), runDir, TURN_RPM, (double)p1 / STEPS_PER_TURN,
           (double)p1 * 360.0 / STEPS_PER_TURN, (long)p1, (long)(p1 - p2), watchdogOn ? "true" : "false");
  sendLine("motor", body);
  lastMotorMsgMs = millis();
}

void sendHello() {
  char body[200];
  snprintf(body, sizeof(body),
           "\"fw\":\"T09\",\"proto\":%d,\"rpm\":%u,\"steps_per_turn\":%lu,\"current_ma\":%u,"
           "\"mirror2\":%s,\"drivers_ok\":%s",
           PROTOCOL_VERSION, TURN_RPM, (unsigned long)STEPS_PER_TURN, MOTOR_CURRENT_MA,
           mirror2 ? "true" : "false", state != State::Off && state != State::Fault ? "true" : "false");
  sendLine("hello", body);
}

void setState(State s) {
  if (s == state) return;
  state = s;
  sendMotor();   // every change is reported at once
}

// ---- Drivers ----------------------------------------------------------------
void motorsPower(bool on) {
  digitalWrite(PIN_EN, on ? LOW : HIGH);
  digitalWrite(PIN_LED_BLUE, on ? HIGH : LOW);
}

bool configureOne(TMC2209Stepper& d) {
  d.begin();
  if (d.test_connection() != 0 || d.version() != EXPECTED_VERSION) return false;
  d.pdn_disable(true);
  d.I_scale_analog(false);
  d.mstep_reg_select(true);
  d.toff(4);
  d.blank_time(24);
  d.microsteps(MICROSTEPS);
  d.rms_current(MOTOR_CURRENT_MA, 0.5f);
  d.en_spreadCycle(false);
  d.pwm_autoscale(true);
  return true;
}

// Try to bring both drivers up. On success: motors hold, counters 0, state idle.
bool bringUpDrivers() {
  motorsPower(false);
  bool ok = configureOne(drivers[0]) && configureOne(drivers[1]);
  if (!ok) return false;
  for (auto* s : steppers) s->setCurrentPosition(0);
  motorsPower(true);
  return true;
}

bool driversHealthy(char* reason, size_t len) {
  for (uint8_t i = 0; i < NUM_MOTORS; i++) {
    TMC2209Stepper& d = drivers[i];
    d.DRV_STATUS();
    if (d.ot()) { snprintf(reason, len, "driver %d over-temperature", i + 1); return false; }
    if (d.s2ga() || d.s2gb() || d.s2vsa() || d.s2vsb()) {
      snprintf(reason, len, "driver %d short circuit", i + 1);
      return false;
    }
  }
  return true;
}

void applyMirror() { steppers[1]->setDirectionPin(PIN_DIR2, !mirror2); }

// ---- Motion -----------------------------------------------------------------
void runBoth() {
  for (auto* s : steppers) { s->setSpeedInHz(STEP_HZ); s->setAcceleration(ACCEL_STEPS_S2); }
  for (auto* s : steppers) { if (runDir > 0) s->runForward(); else s->runBackward(); }
}

void stopBoth() { for (auto* s : steppers) s->stopMove(); }

void resetBoth() {
  for (auto* s : steppers) { s->setSpeedInHz(STEP_HZ); s->setAcceleration(ACCEL_STEPS_S2); }
  for (auto* s : steppers) s->moveTo(0);
}

bool anyRunning() { return steppers[0]->isRunning() || steppers[1]->isRunning(); }
bool moving() { return state == State::Running || state == State::Stopping || state == State::Resetting; }

// ---- Commands ---------------------------------------------------------------
void handleLine(char* line) {
  lastRxMs = millis();              // any line counts for the watchdog
  char cmd = line[0];
  char* arg = line + 1;
  while (*arg == ' ') arg++;
  bool ready = (state != State::Off && state != State::Fault);

  switch (cmd) {
    case '.':                       // keep-alive: no answer
      break;
    case 'S':
      if (!ready) { sendAck('S', false, "drivers not ready (motor power on?)"); break; }
      if (state == State::Resetting) { sendAck('S', false, "wait until reset is finished"); break; }
      if (state == State::Idle) for (auto* s : steppers) s->setCurrentPosition(0);   // counting starts here
      runBoth();
      setState(State::Running);
      sendAck('S', true, "running");
      break;
    case 'P':
      if (state == State::Running || state == State::Resetting) {
        stopBoth();
        setState(State::Stopping);
        sendAck('P', true, "stopping");
      } else {
        sendAck('P', true, "not moving");
      }
      break;
    case 'R':
      runDir = -runDir;
      if (state == State::Running) runBoth();      // slows down, turns, speeds up
      sendAck('R', true, runDir > 0 ? "direction forward" : "direction backward");
      sendMotor();
      break;
    case 'X':
      if (!ready) { sendAck('X', false, "drivers not ready"); break; }
      if (state == State::Idle) { sendAck('X', true, "already at start position"); break; }
      resetBoth();
      setState(State::Resetting);
      sendAck('X', true, "resetting to start position");
      break;
    case 'M':
      if (state != State::Idle && state != State::Off) { sendAck('M', false, "only at the start position (after reset)"); break; }
      mirror2 = !mirror2;
      applyMirror();
      prefs.putBool("mirror2", mirror2);
      sendAck('M', true, mirror2 ? "mirror motor 2 on" : "mirror motor 2 off");
      break;
    case 'D':
      // Re-checking sets the counters to 0, so only allowed when there is nothing to unwind.
      if (state != State::Off && state != State::Idle && state != State::Fault) {
        sendAck('D', false, "only when idle, off or after a fault (reset first)");
        break;
      }
      state = State::Off;
      if (bringUpDrivers()) { setState(State::Idle); sendAck('D', true, "both drivers OK"); }
      else { sendMotor(); sendAck('D', false, "drivers not answering (motor power on? wiring?)"); }
      break;
    case 'W':
      watchdogOn = (*arg != '0');
      sendAck('W', true, watchdogOn ? "watchdog on" : "watchdog off (testing only)");
      break;
    case 'Q': sendMotor(); break;
    case 'i': sendHello(); break;
    case 'h': sendAck('h', true, "S start, P stop, R reverse, X reset, M mirror, D drivers, Q status, W 0/1, ., i, h"); break;
    default:  sendAck(cmd, false, "unknown command (send h)"); break;
  }
}

void readSerialLine() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      lineBuf[lineLen] = '\0';
      if (lineLen > 0) handleLine(lineBuf);
      lineLen = 0;
    } else if (lineLen < sizeof(lineBuf) - 1) {
      lineBuf[lineLen++] = c;
    }
  }
}

// -----------------------------------------------------------------------------
void setup() {
  pinMode(PIN_EN, OUTPUT);
  motorsPower(false);                    // OFF before anything else
  pinMode(PIN_LED_BLUE, OUTPUT);
  Serial.setTxBufferSize(TX_BUFFER_BYTES);
  Serial.begin(BAUD);
  Serial2.begin(UART_BAUD, SERIAL_8N1, PIN_UART_RX, PIN_UART_TX);

  prefs.begin("t08", false);             // same mirror setting as found in T8
  mirror2 = prefs.getBool("mirror2", true);

  engine.init();
  steppers[0] = engine.stepperConnectToPin(PIN_STEP1, DRIVER_RMT);   // RMT, not PCNT (encoder)
  steppers[1] = engine.stepperConnectToPin(PIN_STEP2, DRIVER_RMT);
  steppers[0]->setDirectionPin(PIN_DIR1);
  applyMirror();

  delay(200);
  Serial.println();
  if (bringUpDrivers()) state = State::Idle;
  lastDriverTryMs = millis();
  sendHello();
  sendMotor();
}

void loop() {
  uint32_t now = millis();

  // Drivers not up yet (e.g. motor power switched on later): retry every 3 s.
  if (state == State::Off && now - lastDriverTryMs >= DRIVER_RETRY_MS) {
    lastDriverTryMs = now;
    if (bringUpDrivers()) setState(State::Idle);
  }

  // Finished moving?
  if (state == State::Stopping && !anyRunning()) {
    setState(steppers[0]->getCurrentPosition() == 0 ? State::Idle : State::Paused);
  }
  if (state == State::Resetting && !anyRunning()) {
    setState(steppers[0]->getCurrentPosition() == 0 ? State::Idle : State::Paused);
  }

  // Link watchdog: stop (and hold) if the Pi is silent while the motors move.
  if (watchdogOn && (state == State::Running || state == State::Resetting) && now - lastRxMs > WATCHDOG_MS) {
    stopBoth();
    setState(State::Stopping);
    sendFault("link watchdog: no line from the Pi for 2 s, motors stopped");
  }

  // Driver health while powered.
  if (state != State::Off && state != State::Fault && now - lastHealthMs >= HEALTH_MS) {
    lastHealthMs = now;
    char reason[64];
    if (!driversHealthy(reason, sizeof(reason))) {
      for (auto* s : steppers) s->forceStop();
      motorsPower(false);
      setState(State::Fault);
      sendFault(reason);
    }
  }

  // Regular motor report.
  uint32_t period = moving() ? MOTOR_MOVING_MS : MOTOR_STILL_MS;
  if (now - lastMotorMsgMs >= period) sendMotor();

  readSerialLine();
}
