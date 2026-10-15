// =============================================================================
// T08 - Two TMC2209 drivers on one UART bus + two synchronized motors
// -----------------------------------------------------------------------------
// Builds on T07. Driver 1 = address 0 (MS1 GND), driver 2 = address 1 (MS1 3.3V).
// Both share EN, the UART wire, VM and GND. Each has its own STEP/DIR.
//
// The two motors face each other at the two ends of the cable, so to turn the
// cable as one piece, motor 2 must turn the opposite way to motor 1 (seen from
// each motor's own shaft). "Mirror" does that in software; the setting is saved.
//
// Safety as in T07: motors stay OFF until BOTH drivers answered over UART,
// current from software only (max 900 mA), health flags checked every 0.5 s.
//
// Commands (type, then Enter):
//   i        check both drivers (needs motor power ON)
//   e / x    enable (hold) / disable (free) both motors
//   f / b    both run forward / backward      s  stop     g  both back to 0
//   m        mirror motor 2 on/off (saved)
//   v 30     speed in RPM (1..120)    a 700  current in mA (200..900)
//   p        status    h  help
// =============================================================================

#include <Arduino.h>
#include <TMCStepper.h>
#include <FastAccelStepper.h>
#include <Preferences.h>

// ---- Pins (PIN_PLAN) --------------------------------------------------------
constexpr uint8_t PIN_EN       = 13;   // shared by both drivers, LOW = on
constexpr uint8_t PIN_STEP1    = 25;
constexpr uint8_t PIN_DIR1     = 26;
constexpr uint8_t PIN_STEP2    = 27;
constexpr uint8_t PIN_DIR2     = 14;   // GPIO14 toggles at boot: harmless, drivers are OFF then
constexpr uint8_t PIN_UART_RX  = 4;    // direct to PDN of both drivers
constexpr uint8_t PIN_UART_TX  = 17;   // through 1 kOhm to the same PDN wire
constexpr uint8_t PIN_LED_BLUE = 2;    // on = motors enabled (no RGB LED: RMT is used for steps)

// ---- Drivers ----------------------------------------------------------------
constexpr float    R_SENSE          = 0.11f;
constexpr uint32_t UART_BAUD        = 115200;
constexpr uint8_t  EXPECTED_VERSION = 0x21;
constexpr uint8_t  NUM_MOTORS       = 2;

// ---- Motion -----------------------------------------------------------------
constexpr uint16_t MICROSTEPS         = 16;
constexpr uint32_t STEPS_PER_TURN     = 200UL * MICROSTEPS;
constexpr uint16_t CURRENT_MIN_MA     = 200;
constexpr uint16_t CURRENT_MAX_MA     = 900;
constexpr uint16_t CURRENT_DEFAULT_MA = 700;
constexpr uint16_t RPM_MIN = 1, RPM_MAX = 120, RPM_DEFAULT = 30;
constexpr uint32_t ACCEL_STEPS_S2     = 3200;
constexpr uint32_t STATUS_INTERVAL_MS = 500;
constexpr uint32_t POS_PRINT_MS       = 200;

TMC2209Stepper drivers[NUM_MOTORS] = {
  TMC2209Stepper(&Serial2, R_SENSE, 0b00),   // driver 1: MS1 = GND
  TMC2209Stepper(&Serial2, R_SENSE, 0b01),   // driver 2: MS1 = 3.3 V
};
FastAccelStepperEngine engine;
FastAccelStepper* steppers[NUM_MOTORS] = { nullptr, nullptr };
Preferences prefs;

bool     driversOk  = false;
bool     enabled    = false;
bool     mirror2    = true;    // motor 2 turns opposite (saved)
uint16_t currentMa  = CURRENT_DEFAULT_MA;
uint16_t rpm        = RPM_DEFAULT;
uint32_t lastStatusMs = 0;
uint32_t lastPosPrintMs = 0;
int32_t  lastPrinted[NUM_MOTORS] = { INT32_MIN, INT32_MIN };
char     lineBuf[32];
uint8_t  lineLen = 0;

// -----------------------------------------------------------------------------
uint32_t rpmToHz(uint16_t r) { return (uint32_t)r * STEPS_PER_TURN / 60; }

void setLed(bool on) { digitalWrite(PIN_LED_BLUE, on ? HIGH : LOW); }

void disableMotors(const char* why) {
  for (auto* s : steppers) if (s) s->forceStop();
  digitalWrite(PIN_EN, HIGH);
  enabled = false;
  setLed(false);
  Serial.printf("Motors OFF (%s).\n", why);
}

// Motor 2's direction pin is inverted when mirrored, so both position counts
// stay equal while the shafts turn opposite ways.
void applyMirror() {
  if (steppers[1]) steppers[1]->setDirectionPin(PIN_DIR2, !mirror2);
}

bool configureOne(uint8_t i) {
  TMC2209Stepper& d = drivers[i];
  d.begin();
  uint8_t result = d.test_connection();
  uint8_t version = d.version();
  if (result != 0 || version != EXPECTED_VERSION) {
    Serial.printf("Driver %u: UART FAILED (test %u, version 0x%02X).\n", i + 1, result, version);
    return false;
  }
  d.pdn_disable(true);
  d.I_scale_analog(false);
  d.mstep_reg_select(true);
  d.toff(4);
  d.blank_time(24);
  d.microsteps(MICROSTEPS);
  d.rms_current(currentMa, 0.5f);
  d.en_spreadCycle(false);
  d.pwm_autoscale(true);
  Serial.printf("Driver %u: UART OK (address %u, version 0x%02X), %u mA, %u microsteps.\n",
                i + 1, i, version, d.rms_current(), d.microsteps());
  return true;
}

bool configureDrivers() {
  bool ok1 = configureOne(0);
  bool ok2 = configureOne(1);
  if (!(ok1 && ok2)) {
    Serial.println(F("Check: motor power ON? PDN wire to BOTH drivers? Driver 2 MS1 on 3.3V? Common GND?"));
    if (ok1 && !ok2) Serial.println(F("Driver 1 answers but 2 doesn't: look at driver 2's MS1 (must be 3.3V) and its PDN wire."));
  }
  return ok1 && ok2;
}

bool healthOk(uint8_t i, bool verbose) {
  TMC2209Stepper& d = drivers[i];
  uint32_t drv = d.DRV_STATUS();
  bool ot = d.ot(), otpw = d.otpw();
  bool shorted = d.s2ga() || d.s2gb() || d.s2vsa() || d.s2vsb();
  if (verbose) {
    Serial.printf("Driver %u: DRV_STATUS 0x%08lX  overtemp %s  warn %s  short %s\n", i + 1,
                  (unsigned long)drv, ot ? "YES" : "no", otpw ? "YES" : "no", shorted ? "YES" : "no");
  }
  return !(ot || shorted);
}

void printStatus() {
  int32_t p1 = steppers[0] ? steppers[0]->getCurrentPosition() : 0;
  int32_t p2 = steppers[1] ? steppers[1]->getCurrentPosition() : 0;
  Serial.printf("\nDrivers %s, motors %s, %u mA, %u rpm, mirror %s.\n", driversOk ? "OK" : "NOT CHECKED",
                enabled ? "ON" : "OFF", currentMa, rpm, mirror2 ? "ON" : "OFF");
  Serial.printf("Motor 1: %ld steps (%.2f turns)   Motor 2: %ld steps   difference %ld\n",
                (long)p1, (double)p1 / STEPS_PER_TURN, (long)p2, (long)(p1 - p2));
  if (driversOk) { healthOk(0, true); healthOk(1, true); }
}

void printHelp() {
  Serial.println();
  Serial.println(F("i check drivers | e enable | f forward | b backward | s stop | g go to 0 | x disable"));
  Serial.println(F("m mirror motor 2 | v <rpm> | a <mA> | p status | h help"));
}

bool requireEnabled() {
  if (!driversOk) { Serial.println(F("First: motor power ON, then 'i'.")); return false; }
  if (!enabled)   { Serial.println(F("Motors are OFF. Type 'e' first.")); return false; }
  return true;
}

// Start both motors with identical settings, one right after the other.
void startBoth(char how) {
  for (auto* s : steppers) {
    s->setSpeedInHz(rpmToHz(rpm));
    s->setAcceleration(ACCEL_STEPS_S2);
  }
  for (auto* s : steppers) {
    if (how == 'f') s->runForward();
    else if (how == 'b') s->runBackward();
    else s->moveTo(0);
  }
}

void handleLine(char* line) {
  char cmd = line[0];
  char* arg = line + 1;
  while (*arg == ' ') arg++;
  long value = (*arg != '\0') ? strtol(arg, nullptr, 10) : -1;

  switch (cmd) {
    case 'i':
      disableMotors("re-checking the drivers");
      driversOk = configureDrivers();
      break;
    case 'e':
      if (!driversOk) { Serial.println(F("First: motor power ON, then 'i'.")); break; }
      digitalWrite(PIN_EN, LOW);
      enabled = true;
      setLed(true);
      Serial.println(F("Motors ON (holding)."));
      break;
    case 'x': disableMotors("by command"); break;
    case 'f':
    case 'b':
      if (!requireEnabled()) break;
      startBoth(cmd);
      Serial.printf("Both running %s at %u rpm.\n", cmd == 'f' ? "forward" : "backward", rpm);
      break;
    case 's':
      for (auto* s : steppers) if (s) s->stopMove();
      Serial.println(F("Stopping both."));
      break;
    case 'g':
      if (!requireEnabled()) break;
      startBoth('g');
      Serial.println(F("Both going back to position 0."));
      break;
    case 'm':
      if (steppers[1] && steppers[1]->isRunning()) { Serial.println(F("Stop first ('s').")); break; }
      mirror2 = !mirror2;
      applyMirror();
      prefs.putBool("mirror2", mirror2);
      Serial.printf("Mirror motor 2: %s (saved).\n", mirror2 ? "ON" : "OFF");
      break;
    case 'v':
      if (value < RPM_MIN || value > RPM_MAX) { Serial.printf("Speed must be %u..%u rpm.\n", RPM_MIN, RPM_MAX); break; }
      rpm = (uint16_t)value;
      for (auto* s : steppers) {
        if (s && s->isRunning()) { s->setSpeedInHz(rpmToHz(rpm)); s->applySpeedAcceleration(); }
      }
      Serial.printf("Speed %u rpm.\n", rpm);
      break;
    case 'a':
      if (value < CURRENT_MIN_MA || value > CURRENT_MAX_MA) {
        Serial.printf("Current must be %u..%u mA.\n", CURRENT_MIN_MA, CURRENT_MAX_MA);
        break;
      }
      currentMa = (uint16_t)value;
      if (driversOk) for (auto& d : drivers) d.rms_current(currentMa, 0.5f);
      Serial.printf("Current %u mA RMS (both).\n", currentMa);
      break;
    case 'p': printStatus(); break;
    case 'h': printHelp(); break;
    default: break;
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
  digitalWrite(PIN_EN, HIGH);          // motors OFF first
  pinMode(PIN_LED_BLUE, OUTPUT);
  setLed(false);
  Serial.begin(115200);
  Serial2.begin(UART_BAUD, SERIAL_8N1, PIN_UART_RX, PIN_UART_TX);

  prefs.begin("t08", false);
  mirror2 = prefs.getBool("mirror2", true);

  // RMT step driver (not MCPWM/PCNT): the encoder needs the pulse counter units.
  engine.init();
  steppers[0] = engine.stepperConnectToPin(PIN_STEP1, DRIVER_RMT);
  steppers[1] = engine.stepperConnectToPin(PIN_STEP2, DRIVER_RMT);
  if (steppers[0]) steppers[0]->setDirectionPin(PIN_DIR1);
  applyMirror();

  delay(300);
  Serial.println();
  Serial.println(F("=== T08 two TMC2209 + two motors ==="));
  if (!steppers[0] || !steppers[1]) Serial.println(F("ERROR: step generators not available."));
  Serial.printf("Mirror motor 2: %s\n", mirror2 ? "ON" : "OFF");
  Serial.println(F("Motors are OFF. Switch motor power ON, then type  i  + Enter."));
  printHelp();
}

void loop() {
  uint32_t now = millis();

  if (driversOk && enabled && now - lastStatusMs >= STATUS_INTERVAL_MS) {
    lastStatusMs = now;
    for (uint8_t i = 0; i < NUM_MOTORS; i++) {
      if (!healthOk(i, false)) {
        disableMotors("a driver reported over-temperature or short circuit");
        healthOk(i, true);
        break;
      }
    }
  }

  // Position of both motors + difference, when something changed (max 5 lines/s)
  if (steppers[0] && steppers[1] && now - lastPosPrintMs >= POS_PRINT_MS) {
    lastPosPrintMs = now;
    int32_t p1 = steppers[0]->getCurrentPosition();
    int32_t p2 = steppers[1]->getCurrentPosition();
    if (p1 != lastPrinted[0] || p2 != lastPrinted[1]) {
      lastPrinted[0] = p1;
      lastPrinted[1] = p2;
      Serial.printf("motor 1 %ld  motor 2 %ld  difference %ld steps  (%.2f turns)\n",
                    (long)p1, (long)p2, (long)(p1 - p2), (double)p1 / STEPS_PER_TURN);
    }
  }

  readSerialLine();
}
