// =============================================================================
// T05 - Encoder counts -> slider travel in mm
// -----------------------------------------------------------------------------
// The encoder sits on a GT2 20-tooth pulley. One pulley turn moves the belt
// 20 teeth x 2 mm = 40 mm, and one turn is 8192 counts:
//     nominal mm per count = 40 / 8192 = 0.0048828 mm
// A calibration against a known distance corrects this value (belt stretch,
// real pulley size). Calibration, offset and direction are saved in the
// ESP32's flash (NVS), so they survive power-off.
//
// Wiring: same as T04 (encoder -> 74LVC245 -> GPIO34/35/32).
//
// Commands (type, then Enter):
//   z          zero: slider is at the end stop -> travel = 0
//   o 123.4    offset: cable length when the slider is at zero (mm)
//   c          calibration step 1: slider is at the START mark
//   c 500      calibration step 2: slider moved exactly 500 mm from the start mark
//   d          flip the direction (so pulling the slider out counts +)
//   r          reset calibration to the nominal 40 mm per turn
//   p          print now          h   help
// =============================================================================

#include <Arduino.h>
#include <ESP32Encoder.h>
#include <Preferences.h>

// ---- Pins (confirmed in T3/T4) ----------------------------------------------
constexpr uint8_t PIN_ENC_A    = 34;
constexpr uint8_t PIN_ENC_B    = 35;
constexpr uint8_t PIN_LED_BLUE = 2;
constexpr uint8_t PIN_LED_RGB  = 16;

// ---- Mechanics --------------------------------------------------------------
constexpr int32_t COUNTS_PER_TURN = 2048 * 4;              // DIP all OFF, full quadrature
constexpr double  BELT_PITCH_MM   = 2.0;                   // GT2 belt
constexpr int32_t PULLEY_TEETH    = 20;
constexpr double  MM_PER_TURN     = BELT_PITCH_MM * PULLEY_TEETH;           // 40 mm
constexpr double  NOMINAL_MM_PER_COUNT = MM_PER_TURN / COUNTS_PER_TURN;     // 0.0048828
constexpr double  CAL_MAX_DEVIATION    = 0.10;   // a calibration may change the value by at most 10 %
constexpr int32_t CAL_MIN_COUNTS       = 8192;   // calibrate over at least one turn (40 mm)

constexpr uint16_t PCNT_FILTER       = 100;      // 1.25 us glitch filter (see T04)
constexpr uint32_t PRINT_INTERVAL_MS = 100;
constexpr uint32_t BLINK_INTERVAL_MS = 500;
constexpr uint8_t  RGB_LEVEL         = 20;

ESP32Encoder encoder;
Preferences  prefs;

// ---- Saved settings (NVS) ---------------------------------------------------
double mmPerCount = NOMINAL_MM_PER_COUNT;
double offsetMm   = 0.0;     // cable length when the slider is at zero
int8_t dirSign    = 1;       // +1 or -1

// ---- State ------------------------------------------------------------------
bool     calStarted      = false;
int64_t  calStartCount   = 0;
int64_t  lastPrintCount  = INT64_MIN;
int64_t  lastSpeedCount  = 0;
uint32_t lastSpeedMs     = 0;
uint32_t lastPrintMs     = 0;
uint32_t lastBlinkMs     = 0;
bool     blueLedOn       = false;
char     lineBuf[32];
uint8_t  lineLen         = 0;

// -----------------------------------------------------------------------------
int64_t signedCount() { return dirSign * encoder.getCount(); }
double  travelMm(int64_t count) { return (double)count * mmPerCount; }

void saveSettings() {
  prefs.putDouble("mmpc", mmPerCount);
  prefs.putDouble("off", offsetMm);
  prefs.putChar("dir", dirSign);
}

void printSettings() {
  Serial.println();
  Serial.printf("mm per count : %.7f  (nominal %.7f, %+.3f %%)\n", mmPerCount, NOMINAL_MM_PER_COUNT,
                (mmPerCount / NOMINAL_MM_PER_COUNT - 1.0) * 100.0);
  Serial.printf("mm per turn  : %.4f   (nominal %.1f)\n", mmPerCount * COUNTS_PER_TURN, MM_PER_TURN);
  Serial.printf("offset       : %.2f mm\n", offsetMm);
  Serial.printf("direction    : %s\n", dirSign > 0 ? "normal" : "flipped");
}

void printPosition(int64_t count, double speed) {
  double travel = travelMm(count);
  Serial.printf("travel %9.2f mm   length %9.2f mm   speed %7.1f mm/s   (count %lld, %.1f deg)\n",
                travel, offsetMm + travel, speed, (long long)count,
                (double)count * 360.0 / COUNTS_PER_TURN);
}

void printHelp() {
  Serial.println();
  Serial.println(F("Commands (type, then Enter):"));
  Serial.println(F("  z        zero (slider at the end stop)"));
  Serial.println(F("  o 123.4  set offset = cable length at zero, in mm"));
  Serial.println(F("  c        calibration step 1 (slider at START mark)"));
  Serial.println(F("  c 500    calibration step 2 (moved exactly 500 mm)"));
  Serial.println(F("  d        flip direction    r  reset calibration"));
  Serial.println(F("  p        print now         h  help"));
}

void calibrate(double distanceMm) {
  if (!calStarted) {
    Serial.println(F("First type 'c' alone at the START mark."));
    return;
  }
  int64_t moved = signedCount() - calStartCount;
  int64_t size  = moved < 0 ? -moved : moved;
  if (size < CAL_MIN_COUNTS) {
    Serial.printf("Moved only %lld counts. Move at least one turn (40 mm), more is better.\n", (long long)size);
    return;
  }
  double candidate = distanceMm / (double)size;
  double change = candidate / NOMINAL_MM_PER_COUNT - 1.0;
  if (change > CAL_MAX_DEVIATION || change < -CAL_MAX_DEVIATION) {
    Serial.printf("Rejected: that would change the scale by %+.1f %%. Check the distance you typed.\n", change * 100.0);
    return;
  }
  mmPerCount = candidate;
  calStarted = false;
  saveSettings();
  Serial.printf("Calibrated over %.2f mm (%lld counts).\n", distanceMm, (long long)size);
  printSettings();
}

void handleLine(char* line) {
  char cmd = line[0];
  char* arg = line + 1;
  while (*arg == ' ') arg++;
  bool hasArg = (*arg != '\0');
  double value = hasArg ? strtod(arg, nullptr) : 0.0;

  switch (cmd) {
    case 'z':
      encoder.clearCount();
      lastSpeedCount = 0;
      Serial.println(F("\nZero set: travel = 0 mm."));
      break;
    case 'o':
      if (!hasArg) { Serial.println(F("Type the offset in mm, e.g.  o 123.4")); break; }
      offsetMm = value;
      saveSettings();
      Serial.printf("\nOffset set to %.2f mm (saved).\n", offsetMm);
      break;
    case 'c':
      if (!hasArg) {
        calStartCount = signedCount();
        calStarted = true;
        Serial.println(F("\nCalibration: START mark saved. Move the slider an exact distance,"));
        Serial.println(F("then type  c <distance in mm>  (e.g.  c 500)."));
      } else if (value > 0) {
        calibrate(value);
      } else {
        Serial.println(F("The distance must be a positive number of mm."));
      }
      break;
    case 'd':
      dirSign = -dirSign;
      encoder.setCount(-encoder.getCount());   // keep the current position, flip the sign
      saveSettings();
      Serial.printf("\nDirection %s (saved).\n", dirSign > 0 ? "normal" : "flipped");
      break;
    case 'r':
      mmPerCount = NOMINAL_MM_PER_COUNT;
      saveSettings();
      Serial.println(F("\nCalibration reset to nominal (saved)."));
      printSettings();
      break;
    case 'p':
      printSettings();
      printPosition(signedCount(), 0.0);
      break;
    case 'h':
      printHelp();
      break;
    default:
      break;
  }
}

// Collect typed characters until Enter, then run the command (non-blocking).
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
  Serial.begin(115200);
  delay(300);
  pinMode(PIN_LED_BLUE, OUTPUT);
  neopixelWrite(PIN_LED_RGB, 0, 0, 0);

  prefs.begin("t05", false);
  mmPerCount = prefs.getDouble("mmpc", NOMINAL_MM_PER_COUNT);
  offsetMm   = prefs.getDouble("off", 0.0);
  dirSign    = prefs.getChar("dir", 1) < 0 ? -1 : 1;

  ESP32Encoder::useInternalWeakPullResistors = puType::none;
  encoder.attachFullQuad(PIN_ENC_A, PIN_ENC_B);
  encoder.setFilter(PCNT_FILTER);
  encoder.clearCount();

  Serial.println();
  Serial.println(F("=== T05 encoder -> mm ==="));
  printSettings();
  printHelp();
  Serial.println(F("\nPush the slider to its end stop, then type  z  + Enter."));
}

void loop() {
  uint32_t now = millis();

  if (now - lastPrintMs >= PRINT_INTERVAL_MS) {
    int64_t count = signedCount();
    double dt = (now - lastSpeedMs) / 1000.0;
    double speed = dt > 0 ? travelMm(count - lastSpeedCount) / dt : 0.0;
    lastSpeedCount = count;
    lastSpeedMs = now;
    lastPrintMs = now;
    if (count != lastPrintCount) {     // print only when it moved
      lastPrintCount = count;
      printPosition(count, speed);
    }
  }

  if (now - lastBlinkMs >= BLINK_INTERVAL_MS) {
    lastBlinkMs = now;
    blueLedOn = !blueLedOn;
    digitalWrite(PIN_LED_BLUE, blueLedOn ? HIGH : LOW);
  }

  readSerialLine();
}
