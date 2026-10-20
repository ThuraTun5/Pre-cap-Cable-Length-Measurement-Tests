// =============================================================================
// T10 - Everything together: encoder (slider length) + both clamp-head motors
// -----------------------------------------------------------------------------
// One program, one USB link to the Pi, protocol v2 (docs/PROTOCOL.md):
//   Encoder (from T05/T06):  pos (on change, max 25/s), hb (1/s when still)
//                            commands z o c d r p  (lower case)
//   Motors  (from T09):      motor (on change, 5/s moving, 1/s still), fault
//                            commands S P R X M D Q W  (upper case), "." keep-alive
//   Both:                    hello, ack, commands i h
//
// Hardware resources: encoder = pulse counter (PCNT) unit 0; motor steps = RMT
// channels 0/1 (NOT the MCPWM/PCNT step driver, which would collide with the
// encoder); no RGB LED (it would also need an RMT channel).
//
// Wiring: T4 (encoder + 74LVC245) and T8 (two drivers) on one breadboard; 3.3 V
// and GND through two breadboard rails (atlas tab T10).
// =============================================================================

#include <Arduino.h>
#include <ESP32Encoder.h>
#include <TMCStepper.h>
#include <FastAccelStepper.h>
#include <Preferences.h>

// ---- Pins (PIN_PLAN, confirmed in T3-T8) ------------------------------------
constexpr uint8_t PIN_ENC_A    = 34;
constexpr uint8_t PIN_ENC_B    = 35;
constexpr uint8_t PIN_EN       = 13;
constexpr uint8_t PIN_STEP1    = 25;
constexpr uint8_t PIN_DIR1     = 26;
constexpr uint8_t PIN_STEP2    = 27;
constexpr uint8_t PIN_DIR2     = 14;
constexpr uint8_t PIN_UART_RX  = 4;
constexpr uint8_t PIN_UART_TX  = 17;
constexpr uint8_t PIN_LED_BLUE = 2;    // on = motors powered

// ---- Protocol ---------------------------------------------------------------
constexpr int      PROTOCOL_VERSION = 2;
constexpr uint32_t BAUD             = 115200;
constexpr size_t   TX_BUFFER_BYTES  = 4096;
constexpr uint32_t POS_INTERVAL_MS  = 40;     // encoder pos: max 25/s
constexpr uint32_t HEARTBEAT_MS     = 1000;   // encoder hb when still
constexpr uint32_t MOTOR_MOVING_MS  = 200;    // motor line while moving
constexpr uint32_t MOTOR_STILL_MS   = 1000;   // motor line while still
constexpr uint32_t WATCHDOG_MS      = 2000;   // no line from the Pi for 2 s -> motors stop

// ---- Encoder (T05) ----------------------------------------------------------
constexpr int32_t  COUNTS_PER_TURN      = 2048 * 4;
constexpr double   MM_PER_TURN          = 2.0 * 20;               // GT2 pitch x 20 teeth
constexpr double   NOMINAL_MM_PER_COUNT = MM_PER_TURN / COUNTS_PER_TURN;
constexpr double   CAL_MAX_DEVIATION    = 0.10;
constexpr int32_t  CAL_MIN_COUNTS       = 8192;
constexpr uint16_t PCNT_FILTER          = 100;

// ---- Motors (T09) -----------------------------------------------------------
constexpr float    R_SENSE          = 0.11f;
constexpr uint32_t UART_BAUD        = 115200;
constexpr uint8_t  EXPECTED_VERSION = 0x21;
constexpr uint16_t MICROSTEPS       = 16;
constexpr uint32_t STEPS_PER_TURN   = 200UL * MICROSTEPS;
constexpr uint16_t MOTOR_CURRENT_MA = 700;
constexpr uint16_t TURN_RPM         = 10;                          // constant (to be discussed)
constexpr uint32_t STEP_HZ          = (uint32_t)TURN_RPM * STEPS_PER_TURN / 60;
constexpr uint32_t ACCEL_STEPS_S2   = 3200;
constexpr uint32_t DRIVER_RETRY_MS  = 3000;
constexpr uint32_t HEALTH_MS        = 500;

enum class State { Off, Idle, Running, Stopping, Paused, Resetting, Fault };

const char* stateName(State s) {
  switch (s) {
    case State::Off:       return "off";
    case State::Idle:      return "idle";
    case State::Running:   return "running";
    case State::Stopping:  return "stopping";
    case State::Paused:    return "paused";
    case State::Resetting: return "resetting";
    case State::Fault:     return "fault";
  }
  return "?";
}

ESP32Encoder encoder;
TMC2209Stepper drivers[2] = {
  TMC2209Stepper(&Serial2, R_SENSE, 0b00),
  TMC2209Stepper(&Serial2, R_SENSE, 0b01),
};
FastAccelStepperEngine engine;
FastAccelStepper* steppers[2] = { nullptr, nullptr };
Preferences encPrefs;    // namespace "t05": calibration, offset, direction (from T05/T06)
Preferences motorPrefs;  // namespace "t08": mirror setting (from T08/T09)

// Encoder state
double   mmPerCount = NOMINAL_MM_PER_COUNT;
double   offsetMm   = 0.0;
int8_t   encDir     = 1;
bool     zeroed     = false;
bool     calStarted = false;
int64_t  calStartCount = 0;
int64_t  lastSentCount = INT64_MIN;
int64_t  lastSpeedCount = 0;
uint32_t lastSpeedMs = 0, lastPosMs = 0, lastEncMsgMs = 0;

// Motor state
State    state = State::Off;
int8_t   runDir = 1;
bool     mirror2 = true;
bool     watchdogOn = true;
uint32_t lastMotorMsgMs = 0, lastDriverTryMs = 0, lastHealthMs = 0, lastRxMs = 0;

uint32_t seq = 0;
char     lineBuf[48];
uint8_t  lineLen = 0;

// ============================================================================ output
void sendLine(const char* type, const char* body) {
  seq++;
  Serial.printf("{\"type\":\"%s\",\"seq\":%lu,\"t_ms\":%lu%s%s}\n", type, (unsigned long)seq,
                (unsigned long)millis(), body[0] ? "," : "", body);
}

void sendAck(char cmd, bool ok, const char* msg) {
  char body[200];
  snprintf(body, sizeof(body), "\"cmd\":\"%c\",\"ok\":%s,\"msg\":\"%s\"", cmd, ok ? "true" : "false", msg);
  sendLine("ack", body);
}

void sendFault(const char* reason) {
  char body[160];
  snprintf(body, sizeof(body), "\"reason\":\"%s\"", reason);
  sendLine("fault", body);
}

// ---- encoder messages ----
int64_t encCount() { return encDir * encoder.getCount(); }
double  travelMm(int64_t count) { return (double)count * mmPerCount; }

void sendPosition(const char* type, int64_t count, double speed) {
  char body[220];
  double mm = travelMm(count);
  snprintf(body, sizeof(body),
           "\"count\":%lld,\"deg\":%.2f,\"mm\":%.2f,\"len_mm\":%.2f,\"v_mms\":%.1f,\"zeroed\":%s",
           (long long)count, (double)count * 360.0 / COUNTS_PER_TURN, mm, offsetMm + mm, speed,
           zeroed ? "true" : "false");
  sendLine(type, body);
  lastEncMsgMs = millis();
}

// ---- motor messages ----
void sendMotor() {
  int32_t p1 = steppers[0]->getCurrentPosition();
  int32_t p2 = steppers[1]->getCurrentPosition();
  char body[220];
  snprintf(body, sizeof(body),
           "\"state\":\"%s\",\"dir\":%d,\"rpm\":%u,\"turns\":%.3f,\"clamp_deg\":%.1f,\"steps\":%ld,"
           "\"diff_steps\":%ld,\"watchdog\":%s",
           stateName(state), runDir, TURN_RPM, (double)p1 / STEPS_PER_TURN,
           (double)p1 * 360.0 / STEPS_PER_TURN, (long)p1, (long)(p1 - p2), watchdogOn ? "true" : "false");
  sendLine("motor", body);
  lastMotorMsgMs = millis();
}

void setState(State s) {
  if (s == state) return;
  state = s;
  sendMotor();
}

void sendHello() {
  char body[320];
  snprintf(body, sizeof(body),
           "\"fw\":\"T10\",\"proto\":%d,\"mm_per_count\":%.7f,\"mm_per_turn\":%.4f,\"offset_mm\":%.2f,"
           "\"dir\":%d,\"zeroed\":%s,\"rpm\":%u,\"steps_per_turn\":%lu,\"current_ma\":%u,\"mirror2\":%s,"
           "\"drivers_ok\":%s",
           PROTOCOL_VERSION, mmPerCount, mmPerCount * COUNTS_PER_TURN, offsetMm, encDir,
           zeroed ? "true" : "false", TURN_RPM, (unsigned long)STEPS_PER_TURN, MOTOR_CURRENT_MA,
           mirror2 ? "true" : "false", (state != State::Off && state != State::Fault) ? "true" : "false");
  sendLine("hello", body);
}

// ============================================================================ encoder
void saveEncSettings() {
  encPrefs.putDouble("mmpc", mmPerCount);
  encPrefs.putDouble("off", offsetMm);
  encPrefs.putChar("dir", encDir);
}

void calibrate(double distanceMm) {
  char msg[120];
  if (!calStarted) { sendAck('c', false, "send c alone at the START mark first"); return; }
  int64_t moved = encCount() - calStartCount;
  int64_t size  = moved < 0 ? -moved : moved;
  if (size < CAL_MIN_COUNTS) { sendAck('c', false, "moved less than one turn (40 mm)"); return; }
  double candidate = distanceMm / (double)size;
  double change = candidate / NOMINAL_MM_PER_COUNT - 1.0;
  if (change > CAL_MAX_DEVIATION || change < -CAL_MAX_DEVIATION) {
    snprintf(msg, sizeof(msg), "rejected: scale change %+.1f percent", change * 100.0);
    sendAck('c', false, msg);
    return;
  }
  mmPerCount = candidate;
  calStarted = false;
  saveEncSettings();
  snprintf(msg, sizeof(msg), "calibrated: %.4f mm per turn (%+.3f percent)", mmPerCount * COUNTS_PER_TURN,
           change * 100.0);
  sendAck('c', true, msg);
}

// ============================================================================ motors
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

bool bringUpDrivers() {
  motorsPower(false);
  if (!(configureOne(drivers[0]) && configureOne(drivers[1]))) return false;
  for (auto* s : steppers) s->setCurrentPosition(0);
  motorsPower(true);
  return true;
}

bool driversHealthy(char* reason, size_t len) {
  for (int i = 0; i < 2; i++) {
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

void runBoth() {
  for (auto* s : steppers) { s->setSpeedInHz(STEP_HZ); s->setAcceleration(ACCEL_STEPS_S2); }
  for (auto* s : steppers) { if (runDir > 0) s->runForward(); else s->runBackward(); }
}
void stopBoth()  { for (auto* s : steppers) s->stopMove(); }
void resetBoth() {
  for (auto* s : steppers) { s->setSpeedInHz(STEP_HZ); s->setAcceleration(ACCEL_STEPS_S2); }
  for (auto* s : steppers) s->moveTo(0);
}
bool anyRunning() { return steppers[0]->isRunning() || steppers[1]->isRunning(); }
bool moving() { return state == State::Running || state == State::Stopping || state == State::Resetting; }

// ============================================================================ commands
void handleEncoderCommand(char cmd, bool hasArg, double value) {
  char msg[80];
  switch (cmd) {
    case 'z':
      encoder.clearCount();
      lastSpeedCount = 0;
      zeroed = true;
      sendAck('z', true, "zero set");
      break;
    case 'o':
      if (!hasArg) { sendAck('o', false, "usage: o <mm>"); break; }
      offsetMm = value;
      saveEncSettings();
      snprintf(msg, sizeof(msg), "offset %.2f mm", offsetMm);
      sendAck('o', true, msg);
      break;
    case 'c':
      if (!hasArg) {
        calStartCount = encCount();
        calStarted = true;
        sendAck('c', true, "start mark saved; move a known distance, then send c <mm>");
      } else if (value > 0) {
        calibrate(value);
      } else {
        sendAck('c', false, "distance must be positive");
      }
      break;
    case 'd':
      encDir = -encDir;
      encoder.setCount(-encoder.getCount());
      saveEncSettings();
      sendAck('d', true, encDir > 0 ? "direction normal" : "direction flipped");
      break;
    case 'r':
      mmPerCount = NOMINAL_MM_PER_COUNT;
      saveEncSettings();
      sendAck('r', true, "calibration reset to 40 mm per turn");
      break;
    case 'p':
      sendPosition("pos", encCount(), 0.0);
      break;
  }
}

void handleMotorCommand(char cmd, const char* arg) {
  bool ready = (state != State::Off && state != State::Fault);
  switch (cmd) {
    case 'S':
      if (!ready) { sendAck('S', false, "drivers not ready (motor power on?)"); break; }
      if (state == State::Resetting) { sendAck('S', false, "wait until reset is finished"); break; }
      if (state == State::Idle) for (auto* s : steppers) s->setCurrentPosition(0);
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
      if (state == State::Running) runBoth();
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
      motorPrefs.putBool("mirror2", mirror2);
      sendAck('M', true, mirror2 ? "mirror motor 2 on" : "mirror motor 2 off");
      break;
    case 'D':
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
    case 'Q':
      sendMotor();
      break;
  }
}

void handleLine(char* line) {
  lastRxMs = millis();                    // any line feeds the motor watchdog
  char cmd = line[0];
  char* arg = line + 1;
  while (*arg == ' ') arg++;
  bool hasArg = (*arg != '\0');
  double value = hasArg ? strtod(arg, nullptr) : 0.0;

  if (cmd == '.') return;                 // keep-alive: no answer
  if (strchr("zocdrp", cmd))  { handleEncoderCommand(cmd, hasArg, value); return; }
  if (strchr("SPRXMDWQ", cmd)) { handleMotorCommand(cmd, arg); return; }
  if (cmd == 'i') { sendHello(); return; }
  if (cmd == 'h') {
    sendAck('h', true, "encoder: z, o <mm>, c, c <mm>, d, r, p | motors: S, P, R, X, M, D, Q, W 0/1 | ., i, h");
    return;
  }
  sendAck(cmd, false, "unknown command (send h)");
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

// ============================================================================ setup / loop
void setup() {
  pinMode(PIN_EN, OUTPUT);
  motorsPower(false);                     // motors OFF before anything else
  pinMode(PIN_LED_BLUE, OUTPUT);
  Serial.setTxBufferSize(TX_BUFFER_BYTES);
  Serial.begin(BAUD);
  Serial2.begin(UART_BAUD, SERIAL_8N1, PIN_UART_RX, PIN_UART_TX);

  encPrefs.begin("t05", false);
  mmPerCount = encPrefs.getDouble("mmpc", NOMINAL_MM_PER_COUNT);
  offsetMm   = encPrefs.getDouble("off", 0.0);
  encDir     = encPrefs.getChar("dir", 1) < 0 ? -1 : 1;
  motorPrefs.begin("t08", false);
  mirror2    = motorPrefs.getBool("mirror2", true);

  // Encoder first: it takes pulse-counter unit 0.
  ESP32Encoder::useInternalWeakPullResistors = puType::none;
  encoder.attachFullQuad(PIN_ENC_A, PIN_ENC_B);
  encoder.setFilter(PCNT_FILTER);
  encoder.clearCount();

  // Motors on RMT channels 0/1.
  engine.init();
  steppers[0] = engine.stepperConnectToPin(PIN_STEP1, DRIVER_RMT);
  steppers[1] = engine.stepperConnectToPin(PIN_STEP2, DRIVER_RMT);
  if (!steppers[0] || !steppers[1]) {
    Serial.println(F("{\"type\":\"fault\",\"seq\":0,\"t_ms\":0,\"reason\":\"step generators not available\"}"));
    while (true) delay(1000);   // cannot run motors safely: stop here
  }
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

  // ---- encoder: position on change, heartbeat when still ----
  if (now - lastPosMs >= POS_INTERVAL_MS) {
    int64_t count = encCount();
    double dt = (now - lastSpeedMs) / 1000.0;
    double speed = dt > 0 ? travelMm(count - lastSpeedCount) / dt : 0.0;
    lastSpeedCount = count;
    lastSpeedMs = now;
    lastPosMs = now;
    if (count != lastSentCount) {
      lastSentCount = count;
      sendPosition("pos", count, speed);
    } else if (now - lastEncMsgMs >= HEARTBEAT_MS) {
      sendPosition("hb", count, 0.0);
    }
  }

  // ---- motors ----
  if (state == State::Off && now - lastDriverTryMs >= DRIVER_RETRY_MS) {
    lastDriverTryMs = now;
    if (bringUpDrivers()) setState(State::Idle);
  }
  if ((state == State::Stopping || state == State::Resetting) && !anyRunning()) {
    setState(steppers[0]->getCurrentPosition() == 0 ? State::Idle : State::Paused);
  }
  if (watchdogOn && (state == State::Running || state == State::Resetting) && now - lastRxMs > WATCHDOG_MS) {
    stopBoth();
    setState(State::Stopping);
    sendFault("link watchdog: no line from the Pi for 2 s, motors stopped");
  }
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
  if (now - lastMotorMsgMs >= (moving() ? MOTOR_MOVING_MS : MOTOR_STILL_MS)) sendMotor();

  readSerialLine();
}
