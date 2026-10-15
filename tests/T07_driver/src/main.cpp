// =============================================================================
// T07 - One TMC2209 driver (UART) + one stepper motor
// -----------------------------------------------------------------------------
// Safety first:
//   * The motor stays OFF (EN high) until the driver has answered over UART
//     and its current has been set in software.
//   * The current comes only from software (I_scale_analog = 0), so the blue
//     screw on the driver can't give the motor too much current.
//   * Current is limited to 900 mA (motor rating: 1 A per phase).
//   * The driver's temperature and short-circuit flags are checked twice per
//     second; on any problem the motor is switched off.
//
// Wiring (see the atlas tab T7): EN -> GPIO13 (+10k pull-up to 3V3), STEP -> GPIO25,
// DIR -> GPIO26, UART: GPIO4 (RX) direct to PDN (4th pin), GPIO17 (TX) through 1k
// to the same PDN pin. MS1, MS2, CLK -> GND (address 0, internal clock).
//
// Commands (type, then Enter):
//   i        check the driver over UART (needs motor power ON)
//   e        enable: motor gets current and holds its position
//   f / b    run forward / backward at the set speed
//   s        stop (slows down smoothly)
//   g        go back to the start position (0)
//   x        disable: motor current off, shaft turns freely
//   v 30     speed in RPM (1..120)        a 700   current in mA (200..900)
//   p        print status                 h       help
// =============================================================================

#include <Arduino.h>
#include <TMCStepper.h>
#include <FastAccelStepper.h>

// ---- Pins (PIN_PLAN) --------------------------------------------------------
constexpr uint8_t PIN_EN       = 13;   // LOW = driver on
constexpr uint8_t PIN_STEP     = 25;
constexpr uint8_t PIN_DIR      = 26;
constexpr uint8_t PIN_UART_RX  = 4;    // direct to PDN
constexpr uint8_t PIN_UART_TX  = 17;   // through 1 kOhm to PDN
constexpr uint8_t PIN_LED_BLUE = 2;    // on = motor enabled
// The RGB LED (GPIO16) is NOT used here: neopixelWrite() takes an RMT channel,
// and the step pulses also use RMT channels 0/1 (see setup()).

// ---- Driver -----------------------------------------------------------------
constexpr float    R_SENSE        = 0.11f;   // BTT TMC2209 V1.3: resistors marked "R110"
constexpr uint8_t  DRIVER_ADDRESS = 0b00;    // MS1 = GND, MS2 = GND
constexpr uint32_t UART_BAUD      = 115200;
constexpr uint8_t  EXPECTED_VERSION = 0x21;  // TMC2209 IOIN version field

// ---- Motion -----------------------------------------------------------------
constexpr uint16_t MICROSTEPS      = 16;
constexpr uint32_t STEPS_PER_TURN  = 200UL * MICROSTEPS;   // 1.8 deg motor
constexpr uint16_t CURRENT_MIN_MA  = 200;
constexpr uint16_t CURRENT_MAX_MA  = 900;
constexpr uint16_t CURRENT_DEFAULT_MA = 700;
constexpr uint16_t RPM_MIN = 1, RPM_MAX = 120, RPM_DEFAULT = 30;
constexpr uint32_t ACCEL_STEPS_S2  = 3200;                 // reach 30 rpm in 0.5 s

constexpr uint32_t STATUS_INTERVAL_MS = 500;
constexpr uint32_t POS_PRINT_MS       = 200;   // position line at most 5x per second

TMC2209Stepper driver(&Serial2, R_SENSE, DRIVER_ADDRESS);
FastAccelStepperEngine engine;
FastAccelStepper* stepper = nullptr;

bool     driverOk   = false;   // UART answered and configured
bool     enabled    = false;
uint16_t currentMa  = CURRENT_DEFAULT_MA;
uint16_t rpm        = RPM_DEFAULT;
uint32_t lastStatusMs = 0;
uint32_t lastPosPrintMs = 0;
int32_t  lastPrintedPos = INT32_MIN;
char     lineBuf[32];
uint8_t  lineLen = 0;

// -----------------------------------------------------------------------------
void setLed(bool on) { digitalWrite(PIN_LED_BLUE, on ? HIGH : LOW); }

uint32_t rpmToHz(uint16_t r) { return (uint32_t)r * STEPS_PER_TURN / 60; }

void disableMotor(const char* why) {
  if (stepper) stepper->forceStop();
  digitalWrite(PIN_EN, HIGH);
  enabled = false;
  setLed(false);
  Serial.printf("Motor OFF (%s).\n", why);
}

// Check the UART link and set all registers. Motor power (VM) must be on.
bool configureDriver() {
  driver.begin();
  uint8_t result = driver.test_connection();     // 0 = OK
  uint8_t version = driver.version();
  if (result != 0 || version != EXPECTED_VERSION) {
    Serial.printf("UART FAILED (test %u, version 0x%02X, expected 0x%02X).\n", result, version, EXPECTED_VERSION);
    Serial.println(F("Check: motor power ON? PDN wires (GPIO4 direct, GPIO17 via 1k)? common GND? MS1/MS2 to GND?"));
    return false;
  }
  driver.pdn_disable(true);        // PDN pin is used for UART, not for standstill control
  driver.I_scale_analog(false);    // current from software only, not from the blue screw
  driver.mstep_reg_select(true);   // microsteps from software, not from MS1/MS2
  driver.toff(4);                  // switch the output stage on (0 = off)
  driver.blank_time(24);
  driver.microsteps(MICROSTEPS);
  driver.rms_current(currentMa, 0.5f);   // hold current = 50 % of run current
  driver.en_spreadCycle(false);    // StealthChop: quiet
  driver.pwm_autoscale(true);

  Serial.printf("UART OK: TMC2209 version 0x%02X. Set: %u mA RMS, %u microsteps, StealthChop.\n",
                version, driver.rms_current(), driver.microsteps());
  return true;
}

// Read the driver's warning flags. Returns false if the motor must stop.
bool checkDriverHealth(bool verbose) {
  uint32_t drv = driver.DRV_STATUS();
  bool ot   = driver.ot();        // over-temperature: driver switched itself off
  bool otpw = driver.otpw();      // over-temperature warning
  bool s2g  = driver.s2ga() || driver.s2gb();    // short to GND
  bool s2v  = driver.s2vsa() || driver.s2vsb();  // short to supply
  bool ola  = driver.ola() || driver.olb();      // open load (motor wire missing), only valid while turning
  if (verbose) {
    Serial.printf("Driver: DRV_STATUS 0x%08lX  overtemp %s  warn %s  short %s  open-load %s\n",
                  (unsigned long)drv, ot ? "YES" : "no", otpw ? "YES" : "no",
                  (s2g || s2v) ? "YES" : "no", ola ? "maybe" : "no");
  }
  return !(ot || s2g || s2v);
}

void printStatus() {
  int32_t pos = stepper ? stepper->getCurrentPosition() : 0;
  Serial.printf("\nDriver %s, motor %s, %u mA, %u rpm. Position %ld steps = %.2f turns = %.1f deg\n",
                driverOk ? "OK" : "NOT CHECKED", enabled ? "ON" : "OFF", currentMa, rpm,
                (long)pos, (double)pos / STEPS_PER_TURN, (double)pos * 360.0 / STEPS_PER_TURN);
  if (driverOk) checkDriverHealth(true);
}

void printHelp() {
  Serial.println();
  Serial.println(F("i check driver | e enable | f forward | b backward | s stop | g go to 0"));
  Serial.println(F("x disable | v <rpm> speed | a <mA> current | p status | h help"));
}

bool requireEnabled() {
  if (!driverOk) { Serial.println(F("First: motor power ON, then 'i' to check the driver.")); return false; }
  if (!enabled)  { Serial.println(F("Motor is OFF. Type 'e' first.")); return false; }
  return true;
}

void handleLine(char* line) {
  char cmd = line[0];
  char* arg = line + 1;
  while (*arg == ' ') arg++;
  long value = (*arg != '\0') ? strtol(arg, nullptr, 10) : -1;

  switch (cmd) {
    case 'i':
      disableMotor("re-checking the driver");
      driverOk = configureDriver();
      if (driverOk) checkDriverHealth(true);
      break;
    case 'e':
      if (!driverOk) { Serial.println(F("First: motor power ON, then 'i'.")); break; }
      digitalWrite(PIN_EN, LOW);
      enabled = true;
      setLed(true);
      Serial.println(F("Motor ON (holding). The shaft should now be hard to turn by hand."));
      break;
    case 'x':
      disableMotor("by command");
      break;
    case 'f':
    case 'b':
      if (!requireEnabled()) break;
      stepper->setSpeedInHz(rpmToHz(rpm));
      stepper->setAcceleration(ACCEL_STEPS_S2);
      if (cmd == 'f') stepper->runForward(); else stepper->runBackward();
      Serial.printf("Running %s at %u rpm.\n", cmd == 'f' ? "forward" : "backward", rpm);
      break;
    case 's':
      if (stepper) stepper->stopMove();
      Serial.println(F("Stopping."));
      break;
    case 'g':
      if (!requireEnabled()) break;
      stepper->setSpeedInHz(rpmToHz(rpm));
      stepper->setAcceleration(ACCEL_STEPS_S2);
      stepper->moveTo(0);
      Serial.println(F("Going back to position 0."));
      break;
    case 'v':
      if (value < RPM_MIN || value > RPM_MAX) { Serial.printf("Speed must be %u..%u rpm.\n", RPM_MIN, RPM_MAX); break; }
      rpm = (uint16_t)value;
      if (stepper && stepper->isRunning()) { stepper->setSpeedInHz(rpmToHz(rpm)); stepper->applySpeedAcceleration(); }
      Serial.printf("Speed %u rpm.\n", rpm);
      break;
    case 'a':
      if (value < CURRENT_MIN_MA || value > CURRENT_MAX_MA) {
        Serial.printf("Current must be %u..%u mA.\n", CURRENT_MIN_MA, CURRENT_MAX_MA);
        break;
      }
      currentMa = (uint16_t)value;
      if (driverOk) driver.rms_current(currentMa, 0.5f);
      Serial.printf("Current %u mA RMS.\n", currentMa);
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
  digitalWrite(PIN_EN, HIGH);          // motor OFF first, before anything else
  Serial.begin(115200);
  Serial2.begin(UART_BAUD, SERIAL_8N1, PIN_UART_RX, PIN_UART_TX);
  pinMode(PIN_LED_BLUE, OUTPUT);
  setLed(false);

  engine.init();
  // RMT step driver, not the default MCPWM/PCNT one: the MCPWM/PCNT driver uses
  // pulse-counter units 0..5, which the encoder (ESP32Encoder) also uses.
  stepper = engine.stepperConnectToPin(PIN_STEP, DRIVER_RMT);
  if (stepper) {
    stepper->setDirectionPin(PIN_DIR);
    stepper->setSpeedInHz(rpmToHz(rpm));
    stepper->setAcceleration(ACCEL_STEPS_S2);
  }

  delay(300);
  Serial.println();
  Serial.println(F("=== T07 TMC2209 + one motor ==="));
  if (!stepper) Serial.println(F("ERROR: step generator not available."));
  Serial.println(F("Motor is OFF. Switch motor power ON, then type  i  + Enter."));
  printHelp();
}

void loop() {
  uint32_t now = millis();

  if (driverOk && now - lastStatusMs >= STATUS_INTERVAL_MS) {
    lastStatusMs = now;
    if (enabled && !checkDriverHealth(false)) {
      disableMotor("driver reported over-temperature or short circuit");
      checkDriverHealth(true);
    }
  }

  // Print the position when it changed (at most 5 lines per second)
  if (stepper && now - lastPosPrintMs >= POS_PRINT_MS) {
    lastPosPrintMs = now;
    int32_t pos = stepper->getCurrentPosition();
    if (pos != lastPrintedPos) {
      lastPrintedPos = pos;
      Serial.printf("position %ld steps = %.2f turns\n", (long)pos, (double)pos / STEPS_PER_TURN);
    }
  }

  readSerialLine();
}
