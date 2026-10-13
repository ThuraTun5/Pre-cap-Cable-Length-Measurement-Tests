// =============================================================================
// T03 - 74LVC245 level shifter test, Part B
// -----------------------------------------------------------------------------
// Goal: prove that a 5 V signal on the chip's A side arrives at the ESP32 as a
// clean 3.3 V logic level on the B side, for all three channels the encoder
// will use in T4.
//
//   Test jumper   74LVC245          ESP32 pin   (later in T4)
//   A1 (pin 2)  -> B1 (pin 18)  ->  GPIO35      encoder B
//   A2 (pin 3)  -> B2 (pin 17)  ->  GPIO34      encoder A
//   A3 (pin 4)  -> B3 (pin 16)  ->  GPIO32      encoder X (index)
//
// You move each A jumper between the 5V rail and the GND rail. The serial
// monitor prints a line ONLY when a level changes. The RGB LED mixes the three
// levels: red = A1 HIGH, green = A2 HIGH, blue = A3 HIGH.
//   Start position (A1 = 5V, A2 = 5V, A3 = GND) -> red + green = YELLOW.
//
// Serial commands: p = print all levels now, h = help
// =============================================================================

#include <Arduino.h>

// ---- Pins -------------------------------------------------------------------
constexpr uint8_t PIN_LED_BLUE = 2;    // on-board LED "IO2" (heartbeat)
constexpr uint8_t PIN_LED_RGB  = 16;   // on-board WS2812 RGB LED

struct Channel {
  const char* name;      // which test jumper feeds this channel
  uint8_t     pin;       // ESP32 input pin (B side of the 74LVC245)
  bool        stable;    // last accepted (debounced) level
  bool        raw;       // last raw reading
  uint32_t    rawSinceMs;      // when the raw reading last changed
  uint16_t    rawChanges;      // raw changes counted in the current second
};

// GPIO34/35 are input-only. GPIO32 is a normal pin; we still use plain INPUT
// (no pull-up), because the 74LVC245 output always drives the line.
// GPIO39 is NOT used: on the ESP32 it can be pulled low for ~80 ns when the
// ADC/Wi-Fi powers up (Espressif errata 3.11), which could fake an index pulse.
Channel channels[] = {
  { "A1 -> B1 -> GPIO35", 35, false, false, 0, 0 },
  { "A2 -> B2 -> GPIO34", 34, false, false, 0, 0 },
  { "A3 -> B3 -> GPIO32", 32, false, false, 0, 0 },
};
constexpr size_t NUM_CH = sizeof(channels) / sizeof(channels[0]);

// ---- Timing -----------------------------------------------------------------
constexpr uint32_t SAMPLE_INTERVAL_MS = 1;     // read the pins every 1 ms
constexpr uint32_t DEBOUNCE_MS        = 20;    // a level must stay 20 ms to count
constexpr uint32_t BLINK_INTERVAL_MS  = 500;   // heartbeat
constexpr uint16_t UNSTABLE_LIMIT     = 20;    // > 20 raw changes per second = suspicious
constexpr uint8_t  RGB_LEVEL          = 20;    // 0..255, kept low

uint32_t lastSampleMs = 0;
uint32_t lastBlinkMs  = 0;
uint32_t lastSecondMs = 0;
bool     blueLedOn    = false;

// -----------------------------------------------------------------------------
const char* levelText(bool level) { return level ? "HIGH" : "LOW "; }

void showRgb() {
  neopixelWrite(PIN_LED_RGB,
                channels[0].stable ? RGB_LEVEL : 0,    // red   = A1
                channels[1].stable ? RGB_LEVEL : 0,    // green = A2
                channels[2].stable ? RGB_LEVEL : 0);   // blue  = A3
}

void printAll() {
  Serial.println();
  for (size_t i = 0; i < NUM_CH; i++) {
    Serial.printf("  %s : %s\n", channels[i].name, levelText(channels[i].stable));
  }
}

void printHelp() {
  Serial.println();
  Serial.println(F("Move the A1 / A2 / A3 jumpers between the 5V and GND rails."));
  Serial.println(F("A line is printed only when a level changes."));
  Serial.println(F("Commands: p = print all levels, h = help"));
}

void handleSerialCommand() {
  if (!Serial.available()) return;
  char c = (char)Serial.read();
  if (c == 'p') printAll();
  else if (c == 'h') printHelp();
}

// Read all pins once; accept a new level only after it stayed for DEBOUNCE_MS.
// (Moving a jumper by hand makes the line bounce and float for a moment.)
void sampleChannels(uint32_t now) {
  for (size_t i = 0; i < NUM_CH; i++) {
    Channel& ch = channels[i];
    bool level = digitalRead(ch.pin) == HIGH;
    if (level != ch.raw) {
      ch.raw = level;
      ch.rawSinceMs = now;
      ch.rawChanges++;
    }
    if (ch.raw != ch.stable && now - ch.rawSinceMs >= DEBOUNCE_MS) {
      ch.stable = ch.raw;
      Serial.printf("%s : %s -> %s\n", ch.name, levelText(!ch.stable), levelText(ch.stable));
      showRgb();
    }
  }
}

// Once per second: warn if a pin changed far too often (loose wire or floating input).
void checkStability(uint32_t now) {
  if (now - lastSecondMs < 1000) return;
  lastSecondMs = now;
  for (size_t i = 0; i < NUM_CH; i++) {
    if (channels[i].rawChanges > UNSTABLE_LIMIT) {
      Serial.printf("WARNING %s: %u changes in 1 s -> loose wire or input floating?\n",
                    channels[i].name, channels[i].rawChanges);
    }
    channels[i].rawChanges = 0;
  }
}

// -----------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(300);

  pinMode(PIN_LED_BLUE, OUTPUT);
  for (size_t i = 0; i < NUM_CH; i++) {
    pinMode(channels[i].pin, INPUT);
    channels[i].raw = channels[i].stable = digitalRead(channels[i].pin) == HIGH;
  }

  Serial.println();
  Serial.println(F("=== T03 74LVC245 level shifter test ==="));
  printAll();
  printHelp();
  showRgb();
}

void loop() {
  uint32_t now = millis();

  if (now - lastSampleMs >= SAMPLE_INTERVAL_MS) {
    lastSampleMs = now;
    sampleChannels(now);
  }

  checkStability(now);

  if (now - lastBlinkMs >= BLINK_INTERVAL_MS) {
    lastBlinkMs = now;
    blueLedOn = !blueLedOn;
    digitalWrite(PIN_LED_BLUE, blueLedOn ? HIGH : LOW);
  }

  handleSerialCommand();
}
