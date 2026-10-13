// =============================================================================
// T04 - AMT102-V encoder -> 74LVC245 -> ESP32: counts, degrees, index check
// -----------------------------------------------------------------------------
// Goal: count the encoder in hardware (the ESP32's pulse counter, PCNT) and
// prove that no counts are lost: one full turn must always be 8192 counts.
//
//   Encoder label   74LVC245 leg   ESP32 pin
//   A           ->  3 -> 17     ->  34
//   B           ->  2 -> 18     ->  35
//   X (index)   ->  4 -> 16     ->  32
//   5V          ->  ESP32 5V pin        G -> GND
//
// Encoder DIP switches all OFF = 2048 pulses per turn (factory setting).
// Full quadrature counts every edge of A and B: 2048 x 4 = 8192 counts per turn.
//
// Serial monitor (prints only when the value changes, at most 10x per second):
//   count, angle in degrees, turns
//   INDEX lines: once per turn, with the counts since the last index (should be 8192)
// Commands: z = set count to zero, p = print now, h = help
// =============================================================================

#include <Arduino.h>
#include <ESP32Encoder.h>

// ---- Pins -------------------------------------------------------------------
constexpr uint8_t PIN_ENC_A    = 34;   // via 74LVC245 leg 17 (B2)
constexpr uint8_t PIN_ENC_B    = 35;   // via 74LVC245 leg 18 (B1)
constexpr uint8_t PIN_ENC_X    = 32;   // via 74LVC245 leg 16 (B3); not 39 (ESP32 errata 3.11)
constexpr uint8_t PIN_LED_BLUE = 2;
constexpr uint8_t PIN_LED_RGB  = 16;

// ---- Encoder settings -------------------------------------------------------
constexpr int32_t  PPR             = 2048;      // DIP switches all OFF
constexpr int32_t  COUNTS_PER_TURN = PPR * 4;   // full quadrature = 8192
// PCNT glitch filter, in 80 MHz clock ticks: pulses shorter than this are ignored.
// 100 ticks = 1.25 us. Real edges at a fast hand speed (0.5 m/s) are ~10 us apart.
constexpr uint16_t PCNT_FILTER     = 100;

// ---- Timing -----------------------------------------------------------------
constexpr uint32_t PRINT_INTERVAL_MS = 100;    // at most 10 lines per second
constexpr uint32_t BLINK_INTERVAL_MS = 500;
constexpr uint32_t FLASH_MS          = 150;
constexpr uint8_t  RGB_LEVEL         = 20;

ESP32Encoder encoder;

// Index pulse: the interrupt stores the count at the moment of the pulse.
volatile int64_t indexCount   = 0;
volatile bool    indexPending = false;

int64_t  lastPrintedCount = INT64_MIN;
int64_t  lastIndexCount   = 0;
bool     haveLastIndex    = false;
uint32_t indexEvents      = 0;
uint32_t lastPrintMs      = 0;
uint32_t lastBlinkMs      = 0;
uint32_t flashStartMs     = 0;
bool     flashOn          = false;
bool     blueLedOn        = false;

// -----------------------------------------------------------------------------
void ARDUINO_ISR_ATTR onIndex() {
  indexCount   = encoder.getCount();
  indexPending = true;
}

double countToDegrees(int64_t count) {
  return (double)count * 360.0 / COUNTS_PER_TURN;
}

void printPosition(int64_t count) {
  Serial.printf("count %8lld   angle %9.2f deg   turns %7.3f\n",
                (long long)count, countToDegrees(count), (double)count / COUNTS_PER_TURN);
}

void printHelp() {
  Serial.println();
  Serial.println(F("Turn the encoder by hand. Lines appear only when it moves."));
  Serial.println(F("One full turn = 8192 counts = 360 deg. INDEX = once per turn."));
  Serial.println(F("Commands: z = zero the count, p = print now, h = help"));
}

void handleSerialCommand() {
  if (!Serial.available()) return;
  char c = (char)Serial.read();
  if (c == 'z') {
    encoder.clearCount();
    haveLastIndex = false;
    Serial.println(F("\nCount set to zero."));
  } else if (c == 'p') {
    printPosition(encoder.getCount());
  } else if (c == 'h') {
    printHelp();
  }
}

// Called from loop() when the interrupt saw an index pulse.
void handleIndex() {
  noInterrupts();
  int64_t atIndex = indexCount;
  indexPending = false;
  interrupts();

  indexEvents++;
  neopixelWrite(PIN_LED_RGB, 0, RGB_LEVEL, 0);   // green flash = index seen
  flashOn = true;
  flashStartMs = millis();

  if (!haveLastIndex) {
    Serial.printf("INDEX #%lu at count %lld (first one: reference)\n",
                  (unsigned long)indexEvents, (long long)atIndex);
  } else {
    int64_t delta = atIndex - lastIndexCount;
    int64_t size  = delta < 0 ? -delta : delta;
    if (size == 0) {
      // Turned back and forth across the index: no full turn in between.
      Serial.printf("INDEX #%lu at count %lld (same spot again)\n",
                    (unsigned long)indexEvents, (long long)atIndex);
    } else {
      // A full turn in one direction must be exactly 8192 counts.
      // A few counts difference is normal if the direction changed near the index.
      bool ok = (size >= COUNTS_PER_TURN - 4 && size <= COUNTS_PER_TURN + 4);
      Serial.printf("INDEX #%lu at count %lld  -> %lld counts since last index  %s\n",
                    (unsigned long)indexEvents, (long long)atIndex, (long long)delta,
                    ok ? "OK (one full turn)" : "CHECK: not one full turn");
    }
  }
  lastIndexCount = atIndex;
  haveLastIndex  = true;
}

// -----------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(300);
  pinMode(PIN_LED_BLUE, OUTPUT);
  neopixelWrite(PIN_LED_RGB, 0, 0, 0);

  // GPIO34/35 have no internal pull resistors, and the 74LVC245 drives the lines,
  // so no pull-ups are wanted.
  ESP32Encoder::useInternalWeakPullResistors = puType::none;
  encoder.attachFullQuad(PIN_ENC_A, PIN_ENC_B);
  encoder.setFilter(PCNT_FILTER);
  encoder.clearCount();

  pinMode(PIN_ENC_X, INPUT);
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_X), onIndex, RISING);

  Serial.println();
  Serial.println(F("=== T04 AMT102-V encoder test ==="));
  Serial.printf("Resolution: %ld PPR x 4 = %ld counts per turn\n", (long)PPR, (long)COUNTS_PER_TURN);
  printHelp();
  printPosition(encoder.getCount());
}

void loop() {
  uint32_t now = millis();

  // Print the position when it changed, but at most every PRINT_INTERVAL_MS.
  // (By hand the count can change thousands of times per second.)
  if (now - lastPrintMs >= PRINT_INTERVAL_MS) {
    int64_t count = encoder.getCount();
    if (count != lastPrintedCount) {
      lastPrintedCount = count;
      lastPrintMs = now;
      printPosition(count);
    }
  }

  if (indexPending) handleIndex();

  if (flashOn && now - flashStartMs >= FLASH_MS) {
    flashOn = false;
    neopixelWrite(PIN_LED_RGB, 0, 0, 0);
  }

  if (now - lastBlinkMs >= BLINK_INTERVAL_MS) {
    lastBlinkMs = now;
    blueLedOn = !blueLedOn;
    digitalWrite(PIN_LED_BLUE, blueLedOn ? HIGH : LOW);
  }

  handleSerialCommand();
}
